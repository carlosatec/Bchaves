/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Secp256k1 elliptic curve arithmetic optimized for SSE4.1.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__SSE4_1__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include "core/secp256k1_reduce.hpp"

namespace bchaves::core::secp256k1_sse4 {

typedef __m128i fe_sse4[4];

struct ProjectivePoint {
    fe_sse4 x;
    fe_sse4 y;
    fe_sse4 z;
};

/**
 * Multiplicação 64x64 -> 128 bits usando SSE4.1 com propagação de carry.
 */
inline void mul64x64_full(__m128i a, __m128i b, __m128i& r_lo, __m128i& r_hi) {
    __m128i mask32 = _mm_set1_epi64x(0xFFFFFFFFULL);
    __m128i a_lo = _mm_and_si128(a, mask32);
    __m128i a_hi = _mm_srli_epi64(a, 32);
    __m128i b_lo = _mm_and_si128(b, mask32);
    __m128i b_hi = _mm_srli_epi64(b, 32);

    __m128i ll = _mm_mul_epu32(a_lo, b_lo);
    __m128i lh = _mm_mul_epu32(a_lo, b_hi);
    __m128i hl = _mm_mul_epu32(a_hi, b_lo);
    __m128i hh = _mm_mul_epu32(a_hi, b_hi);

    __m128i mid = _mm_add_epi64(lh, hl);
    __m128i mid_lo = _mm_slli_epi64(mid, 32);
    __m128i mid_hi = _mm_srli_epi64(mid, 32);

    r_lo = _mm_add_epi64(ll, mid_lo);
    
    __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
    __m128i r_lo_s = _mm_xor_si128(r_lo, sign_mask);
    __m128i ll_s = _mm_xor_si128(ll, sign_mask);
    __m128i carry = _mm_cmpgt_epi64(ll_s, r_lo_s); // -1 if r_lo < ll
    
    r_hi = _mm_add_epi64(hh, mid_hi);
    r_hi = _mm_sub_epi64(r_hi, carry); // subtract -1 (adds 1)
}

/**
 * Adição 256-bit com propagação de carry.
 */
inline void add_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
    __m128i carry = _mm_setzero_si128();

    for (int i = 0; i < 4; ++i) {
        __m128i ai = a[i];
        __m128i bi = b[i];
        __m128i sum = _mm_add_epi64(ai, bi);
        sum = _mm_sub_epi64(sum, carry);
        
        __m128i sum_s = _mm_xor_si128(sum, sign_mask);
        __m128i ai_s = _mm_xor_si128(ai, sign_mask);
        carry = _mm_cmpgt_epi64(ai_s, sum_s); 
        r[i] = sum;
    }
}

/**
 * Subtração 256-bit com propagação de borrow.
 */
inline void sub_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
    __m128i borrow = _mm_setzero_si128();

    for (int i = 0; i < 4; ++i) {
        __m128i ai = a[i];
        __m128i bi = b[i];
        __m128i diff = _mm_sub_epi64(ai, bi);
        diff = _mm_add_epi64(diff, borrow);
        
        __m128i ai_s = _mm_xor_si128(ai, sign_mask);
        __m128i bi_s = _mm_xor_si128(bi, sign_mask);
        borrow = _mm_cmpgt_epi64(bi_s, ai_s);
        r[i] = diff;
    }
}

inline void add_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    add_sse4(r, a, b);
}

inline void sub_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    fe_sse4 p_limbs = {
        _mm_set1_epi64x(0xFFFFFFFFFFFFFF43ULL),
        _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm_set1_epi64x(0xFFFFFFFEFFFFFFFFULL)
    };
    fe_sse4 t;
    add_sse4(t, a, p_limbs);
    sub_sse4(r, t, b);
}

inline void mul_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    __m128i acc[8];
    for(int i=0; i<8; ++i) acc[i] = _mm_setzero_si128();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            __m128i lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
            __m128i old_acc = acc[i+j];
            acc[i+j] = _mm_add_epi64(acc[i+j], lo);
            __m128i c = _mm_cmpgt_epi64(_mm_xor_si128(old_acc, sign_mask), _mm_xor_si128(acc[i+j], sign_mask));
            
            __m128i hi_plus_carry = _mm_sub_epi64(hi, c);
            int k = i + j + 1;
            while (k < 8) {
                __m128i old_k = acc[k];
                acc[k] = _mm_add_epi64(acc[k], hi_plus_carry);
                __m128i ck = _mm_cmpgt_epi64(_mm_xor_si128(old_k, sign_mask), _mm_xor_si128(acc[k], sign_mask));
                if (_mm_movemask_epi8(ck) == 0) break;
                hi_plus_carry = _mm_set1_epi64x(1);
                k++;
            }
        }
    }

    // Modular Reduction: 2^256 = 2^32 + 977 mod P
    __m128i k_977 = _mm_set1_epi64x(977);
    for (int i = 0; i < 4; ++i) {
        __m128i h = acc[i+4];
        
        // h * 977
        __m128i p_lo, p_hi;
        mul64x64_full(h, k_977, p_lo, p_hi);
        
        // h * 2^32
        __m128i s_lo = _mm_slli_epi64(h, 32);
        __m128i s_hi = _mm_srli_epi64(h, 32);
        
        fe_sse4 to_add = { _mm_setzero_si128(), _mm_setzero_si128(), _mm_setzero_si128(), _mm_setzero_si128() };
        to_add[0] = _mm_add_epi64(p_lo, s_lo);
        // Carry from to_add[0]
        __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
        __m128i c0 = _mm_cmpgt_epi64(_mm_xor_si128(p_lo, sign_mask), _mm_xor_si128(to_add[0], sign_mask));
        
        to_add[1] = _mm_add_epi64(p_hi, s_hi);
        to_add[1] = _mm_sub_epi64(to_add[1], c0);
        
        // Add to_add to acc limbs
        fe_sse4 current_acc;
        for(int l=0; l<4; ++l) current_acc[l] = acc[l];
        add_sse4(current_acc, current_acc, to_add);
        for(int l=0; l<4; ++l) acc[l] = current_acc[l];
    }
    
    for(int i=0; i<4; ++i) r[i] = acc[i];
}

inline void square_mod_sse4(fe_sse4& r, const fe_sse4& a) {
    mul_mod_sse4(r, a, a);
}

inline ProjectivePoint point_add_sse4(ProjectivePoint p1, ProjectivePoint p2) {
    ProjectivePoint res;
    fe_sse4 z1_2, z2_2, u1, u2, s1, s2, h, r_val, h2, h3, u1h2;

    square_mod_sse4(z1_2, p1.z);
    square_mod_sse4(z2_2, p2.z);
    mul_mod_sse4(u1, p1.x, z2_2);
    mul_mod_sse4(u2, p2.x, z1_2);

    fe_sse4 z1_3, z2_3;
    mul_mod_sse4(z1_3, z1_2, p1.z);
    mul_mod_sse4(z2_3, z2_2, p2.z);
    mul_mod_sse4(s1, p1.y, z2_3);
    mul_mod_sse4(s2, p2.y, z1_3);

    sub_mod_sse4(h, u2, u1);
    sub_mod_sse4(r_val, s2, s1);

    fe_sse4 z1z2;
    mul_mod_sse4(z1z2, p1.z, p2.z);
    mul_mod_sse4(res.z, z1z2, h);

    square_mod_sse4(h2, h);
    mul_mod_sse4(h3, h2, h);
    mul_mod_sse4(u1h2, u1, h2);
    
    fe_sse4 r2, u1h2_2;
    square_mod_sse4(r2, r_val);
    add_mod_sse4(u1h2_2, u1h2, u1h2);
    
    sub_mod_sse4(res.x, r2, h3);
    sub_mod_sse4(res.x, res.x, u1h2_2);

    fe_sse4 u1h2_x3, r_u1h2_x3, s1h3;
    sub_mod_sse4(u1h2_x3, u1h2, res.x);
    mul_mod_sse4(r_u1h2_x3, r_val, u1h2_x3);
    mul_mod_sse4(s1h3, s1, h3);
    sub_mod_sse4(res.y, r_u1h2_x3, s1h3);

    return res;
}

}  // namespace bchaves::core::secp256k1_sse4

#endif  // defined(__SSE4_1__)