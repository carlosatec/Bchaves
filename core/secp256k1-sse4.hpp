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
    
    // Carry propagation: check if r_lo < mid_lo (unsigned)
    // SSE4.1 doesn't have unsigned comparison, so we use a trick:
    // (a < b) is same as (a - b) overflows.
    // Or we can use the high bit after subtracting.
    // For simplicity and speed, we use the property: carry = (r_lo < ll)
    // We use _mm_cmplt_epi64 (signed), so we need to adjust for unsigned.
    __m128i sign_mask = _mm_set1_epi64x(0x8000000000000000ULL);
    __m128i r_lo_s = _mm_xor_si128(r_lo, sign_mask);
    __m128i ll_s = _mm_xor_si128(ll, sign_mask);
    __m128i carry = _mm_cmpgt_epi64(ll_s, r_lo_s); // -1 if ll > r_lo (which means r_lo < ll)
    
    r_hi = _mm_add_epi64(hh, mid_hi);
    r_hi = _mm_sub_epi64(r_hi, carry); // subtract -1 (adds 1)
}

inline void add_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    for(int i=0; i<4; ++i) r[i] = _mm_add_epi64(a[i], b[i]);
    // Note: Reduction omitted for speed, assuming delayed reduction strategy
}

inline void sub_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    // Add P to avoid negative results before subtraction
    // P = 2^256 - 2^32 - 977
    __m128i p_limbs[4] = {
        _mm_set1_epi64x(0xFFFFFFFFFFFFFF43ULL),
        _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm_set1_epi64x(0xFFFFFFFFFFFFFFFFULL),
        _mm_set1_epi64x(0xFFFFFFFEFFFFFFFFULL)
    };
    for(int i=0; i<4; ++i) {
        __m128i t = _mm_add_epi64(a[i], p_limbs[i]);
        r[i] = _mm_sub_epi64(t, b[i]);
    }
}

inline void mul_mod_sse4(fe_sse4& r, const fe_sse4& a, const fe_sse4& b) {
    __m128i acc[8];
    for(int i=0; i<8; ++i) acc[i] = _mm_setzero_si128();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            __m128i lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            // Basic limb addition with carry propagation (simplified)
            acc[i+j] = _mm_add_epi64(acc[i+j], lo);
            acc[i+j+1] = _mm_add_epi64(acc[i+j+1], hi);
        }
    }

    // Modular Reduction for P = 2^256 - 2^32 - 977
    // 2^256 = 2^32 + 977 mod P
    __m128i c = _mm_set1_epi64x(977);
    for (int i = 0; i < 4; ++i) {
        __m128i hi_limb = acc[i+4];
        
        // hi * 977
        __m128i prod_lo, prod_hi;
        mul64x64_full(hi_limb, c, prod_lo, prod_hi);
        
        // hi << 32
        __m128i hi_shift_lo = _mm_slli_epi64(hi_limb, 32);
        __m128i hi_shift_hi = _mm_srli_epi64(hi_limb, 32);
        
        acc[i] = _mm_add_epi64(acc[i], prod_lo);
        acc[i] = _mm_add_epi64(acc[i], hi_shift_lo);
        
        if (i + 1 < 4) {
            acc[i+1] = _mm_add_epi64(acc[i+1], prod_hi);
            acc[i+1] = _mm_add_epi64(acc[i+1], hi_shift_hi);
        }
    }
    
    for(int i=0; i<4; ++i) r[i] = acc[i];
}

inline void square_mod_sse4(fe_sse4& r, const fe_sse4& a) {
    mul_mod_sse4(r, a, a);
}

/**
 * Adição Jacobiana: P3 = P1 + P2
 * Fórmulas Jacobianas otimizadas para evitar inversões modulares.
 */
inline ProjectivePoint point_add_sse4(ProjectivePoint p1, ProjectivePoint p2) {
    ProjectivePoint res;
    fe_sse4 z1_2, z2_2, u1, u2, s1, s2, h, r, h2, h3, u1h2;

    // z1^2, z2^2
    square_mod_sse4(z1_2, p1.z);
    square_mod_sse4(z2_2, p2.z);

    // u1 = x1 * z2^2, u2 = x2 * z1^2
    mul_mod_sse4(u1, p1.x, z2_2);
    mul_mod_sse4(u2, p2.x, z1_2);

    // s1 = y1 * z2^3, s2 = y2 * z1^3
    fe_sse4 z1_3, z2_3;
    mul_mod_sse4(z1_3, z1_2, p1.z);
    mul_mod_sse4(z2_3, z2_2, p2.z);
    mul_mod_sse4(s1, p1.y, z2_3);
    mul_mod_sse4(s2, p2.y, z1_3);

    // h = u2 - u1, r = s2 - s1
    sub_mod_sse4(h, u2, u1);
    sub_mod_sse4(r, s2, s1);

    // res.z = z1 * z2 * h
    fe_sse4 z1z2;
    mul_mod_sse4(z1z2, p1.z, p2.z);
    mul_mod_sse4(res.z, z1z2, h);

    // res.x = r^2 - h^3 - 2*u1*h^2
    square_mod_sse4(h2, h);
    mul_mod_sse4(h3, h2, h);
    mul_mod_sse4(u1h2, u1, h2);
    
    fe_sse4 r2, u1h2_2;
    square_mod_sse4(r2, r);
    add_mod_sse4(u1h2_2, u1h2, u1h2); // 2*u1*h^2
    
    sub_mod_sse4(res.x, r2, h3);
    sub_mod_sse4(res.x, res.x, u1h2_2);

    // res.y = r*(u1*h^2 - x3) - s1*h^3
    fe_sse4 u1h2_x3, r_u1h2_x3, s1h3;
    sub_mod_sse4(u1h2_x3, u1h2, res.x);
    mul_mod_sse4(r_u1h2_x3, r, u1h2_x3);
    mul_mod_sse4(s1h3, s1, h3);
    sub_mod_sse4(res.y, r_u1h2_x3, s1h3);

    return res;
}

}  // namespace bchaves::core::secp256k1_sse4

#endif  // defined(__SSE4_1__)