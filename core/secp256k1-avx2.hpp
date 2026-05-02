/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Secp256k1 elliptic curve arithmetic optimized for AVX2.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__AVX2__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include "core/secp256k1_reduce.hpp"

namespace bchaves::core::secp256k1_avx2 {

typedef __m256i fe_avx2[4];

struct ProjectivePoint {
    fe_avx2 x;
    fe_avx2 y;
    fe_avx2 z;
};

/**
 * Multiplicação 64x64 -> 128 bits usando AVX2 com propagação de carry.
 */
inline void mul64x64_full(__m256i a, __m256i b, __m256i& r_lo, __m256i& r_hi) {
    __m256i mask32 = _mm256_set1_epi64x(0xFFFFFFFFULL);
    __m256i a_lo = _mm256_and_si256(a, mask32);
    __m256i a_hi = _mm256_srli_epi64(a, 32);
    __m256i b_lo = _mm256_and_si256(b, mask32);
    __m256i b_hi = _mm256_srli_epi64(b, 32);

    __m256i ll = _mm256_mul_epu32(a_lo, b_lo);
    __m256i lh = _mm256_mul_epu32(a_lo, b_hi);
    __m256i hl = _mm256_mul_epu32(a_hi, b_lo);
    __m256i hh = _mm256_mul_epu32(a_hi, b_hi);

    __m256i mid = _mm256_add_epi64(lh, hl);
    __m256i mid_lo = _mm256_slli_epi64(mid, 32);
    __m256i mid_hi = _mm256_srli_epi64(mid, 32);

    r_lo = _mm256_add_epi64(ll, mid_lo);
    
    __m256i sign_mask = _mm256_set1_epi64x(0x8000000000000000ULL);
    __m256i r_lo_s = _mm256_xor_si256(r_lo, sign_mask);
    __m256i ll_s = _mm256_xor_si256(ll, sign_mask);
    __m256i carry = _mm256_cmpgt_epi64(ll_s, r_lo_s);
    
    r_hi = _mm256_add_epi64(hh, mid_hi);
    r_hi = _mm256_sub_epi64(r_hi, carry);
}

inline void add_avx2(fe_avx2& r, const fe_avx2& a, const fe_avx2& b) {
    __m256i sign_mask = _mm256_set1_epi64x(0x8000000000000000ULL);
    __m256i carry = _mm256_setzero_si256();
    for (int i = 0; i < 4; ++i) {
        __m256i sum = _mm256_add_epi64(a[i], b[i]);
        sum = _mm256_sub_epi64(sum, carry);
        carry = _mm256_cmpgt_epi64(_mm256_xor_si256(a[i], sign_mask), _mm256_xor_si256(sum, sign_mask));
        r[i] = sum;
    }
}

inline void sub_avx2(fe_avx2& r, const fe_avx2& a, const fe_avx2& b) {
    __m256i sign_mask = _mm256_set1_epi64x(0x8000000000000000ULL);
    __m256i borrow = _mm256_setzero_si256();
    for (int i = 0; i < 4; ++i) {
        __m256i diff = _mm256_sub_epi64(a[i], b[i]);
        diff = _mm256_add_epi64(diff, borrow);
        borrow = _mm256_cmpgt_epi64(_mm256_xor_si256(b[i], sign_mask), _mm256_xor_si256(a[i], sign_mask));
        r[i] = diff;
    }
}

inline void add_mod_avx2(fe_avx2& r, const fe_avx2& a, const fe_avx2& b) {
    add_avx2(r, a, b);
}

inline void sub_mod_avx2(fe_avx2& r, const fe_avx2& a, const fe_avx2& b) {
    fe_avx2 p_limbs = {
        _mm256_set1_epi64x(0xFFFFFFFFFFFFFF43ULL),
        _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm256_set1_epi64x(0xFFFFFFFEFFFFFFFFULL)
    };
    fe_avx2 t;
    add_avx2(t, a, p_limbs);
    sub_avx2(r, t, b);
}

inline void mul_mod_avx2(fe_avx2& r, const fe_avx2& a, const fe_avx2& b) {
    __m256i acc[8];
    for(int i=0; i<8; ++i) acc[i] = _mm256_setzero_si256();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            __m256i lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            __m256i sign_mask = _mm256_set1_epi64x(0x8000000000000000ULL);
            __m256i old_acc = acc[i+j];
            acc[i+j] = _mm256_add_epi64(acc[i+j], lo);
            __m256i c = _mm256_cmpgt_epi64(_mm256_xor_si256(old_acc, sign_mask), _mm256_xor_si256(acc[i+j], sign_mask));
            
            __m256i hi_plus_carry = _mm256_sub_epi64(hi, c);
            int k = i + j + 1;
            while (k < 8) {
                __m256i old_k = acc[k];
                acc[k] = _mm256_add_epi64(acc[k], hi_plus_carry);
                __m256i ck = _mm256_cmpgt_epi64(_mm256_xor_si256(old_k, sign_mask), _mm256_xor_si256(acc[k], sign_mask));
                if (_mm256_testz_si256(ck, ck)) break;
                hi_plus_carry = _mm256_set1_epi64x(1);
                k++;
            }
        }
    }
    
    // Reduction
    __m256i c_val = _mm256_set1_epi64x(977);
    for (int i = 0; i < 4; ++i) {
        __m256i h = acc[i+4];
        __m256i p_lo, p_hi;
        mul64x64_full(h, c_val, p_lo, p_hi);
        __m256i s_lo = _mm256_slli_epi64(h, 32);
        __m256i s_hi = _mm256_srli_epi64(h, 32);
        
        fe_avx2 to_add = { _mm256_setzero_si256() };
        to_add[0] = _mm256_add_epi64(p_lo, s_lo);
        __m256i sign_mask = _mm256_set1_epi64x(0x8000000000000000ULL);
        __m256i c0 = _mm256_cmpgt_epi64(_mm256_xor_si256(p_lo, sign_mask), _mm256_xor_si256(to_add[0], sign_mask));
        to_add[1] = _mm256_sub_epi64(_mm256_add_epi64(p_hi, s_hi), c0);
        
        fe_avx2 current_acc;
        for(int l=0; l<4; ++l) current_acc[l] = acc[l];
        add_avx2(current_acc, current_acc, to_add);
        for(int l=0; l<4; ++l) acc[l] = current_acc[l];
    }

    for(int i=0; i<4; ++i) r[i] = acc[i];
}

inline void square_mod_avx2(fe_avx2& r, const fe_avx2& a) {
    mul_mod_avx2(r, a, a);
}

inline ProjectivePoint point_add_avx2(ProjectivePoint p1, ProjectivePoint p2) {
    ProjectivePoint res;
    fe_avx2 z1_2, z2_2, u1, u2, s1, s2, h, r_val, h2, h3, u1h2;

    square_mod_avx2(z1_2, p1.z);
    square_mod_avx2(z2_2, p2.z);
    mul_mod_avx2(u1, p1.x, z2_2);
    mul_mod_avx2(u2, p2.x, z1_2);

    fe_avx2 z1_3, z2_3;
    mul_mod_avx2(z1_3, z1_2, p1.z);
    mul_mod_avx2(z2_3, z2_2, p2.z);
    mul_mod_avx2(s1, p1.y, z2_3);
    mul_mod_avx2(s2, p2.y, z1_3);

    sub_mod_avx2(h, u2, u1);
    sub_mod_avx2(r_val, s2, s1);

    fe_avx2 z1z2;
    mul_mod_avx2(z1z2, p1.z, p2.z);
    mul_mod_avx2(res.z, z1z2, h);

    square_mod_avx2(h2, h);
    mul_mod_avx2(h3, h2, h);
    mul_mod_avx2(u1h2, u1, h2);
    
    fe_avx2 r2, u1h2_2;
    square_mod_avx2(r2, r_val);
    add_mod_avx2(u1h2_2, u1h2, u1h2);
    
    sub_mod_avx2(res.x, r2, h3);
    sub_mod_avx2(res.x, res.x, u1h2_2);

    fe_avx2 u1h2_x3, r_u1h2_x3, s1h3;
    sub_mod_avx2(u1h2_x3, u1h2, res.x);
    mul_mod_avx2(r_u1h2_x3, r_val, u1h2_x3);
    mul_mod_avx2(s1h3, s1, h3);
    sub_mod_avx2(res.y, r_u1h2_x3, s1h3);

    return res;
}

}  // namespace bchaves::core::secp256k1_avx2

#endif  // defined(__AVX2__)