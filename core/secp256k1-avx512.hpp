/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Secp256k1 elliptic curve arithmetic optimized for AVX512.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__AVX512F__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include "core/secp256k1_reduce.hpp"

namespace bchaves::core::secp256k1_avx512 {

typedef __m512i fe_avx512[4];

struct ProjectivePoint {
    fe_avx512 x;
    fe_avx512 y;
    fe_avx512 z;
};

inline void mul64x64_full(__m512i a, __m512i b, __m512i& r_lo, __m512i& r_hi) {
    __m512i mask32 = _mm512_set1_epi64(0xFFFFFFFFULL);
    __m512i a_lo = _mm512_and_si512(a, mask32);
    __m512i a_hi = _mm512_srli_epi64(a, 32);
    __m512i b_lo = _mm512_and_si512(b, mask32);
    __m512i b_hi = _mm512_srli_epi64(b, 32);

    __m512i ll = _mm512_mul_epu32(a_lo, b_lo);
    __m512i lh = _mm512_mul_epu32(a_lo, b_hi);
    __m512i hl = _mm512_mul_epu32(a_hi, b_lo);
    __m512i hh = _mm512_mul_epu32(a_hi, b_hi);

    __m512i mid = _mm512_add_epi64(lh, hl);
    __m512i mid_lo = _mm512_slli_epi64(mid, 32);
    __m512i mid_hi = _mm512_srli_epi64(mid, 32);

    r_lo = _mm512_add_epi64(ll, mid_lo);
    
    __mmask8 carry_mask = _mm512_cmputail_epu64_mask(r_lo, ll);
    r_hi = _mm512_add_epi64(hh, mid_hi);
    r_hi = _mm512_mask_add_epi64(r_hi, carry_mask, r_hi, _mm512_set1_epi64(1));
}

inline void add_avx512(fe_avx512& r, const fe_avx512& a, const fe_avx512& b) {
    __mmask8 carry = 0;
    for (int i = 0; i < 4; ++i) {
        __m512i sum = _mm512_add_epi64(a[i], b[i]);
        if (carry) sum = _mm512_add_epi64(sum, _mm512_set1_epi64(1));
        
        __mmask8 c1 = _mm512_cmputail_epu64_mask(sum, a[i]);
        __mmask8 c2 = _mm512_cmpeq_epu64_mask(sum, a[i]);
        carry = c1 | (c2 & carry);
        r[i] = sum;
    }
}

inline void sub_avx512(fe_avx512& r, const fe_avx512& a, const fe_avx512& b) {
    __mmask8 borrow = 0;
    for (int i = 0; i < 4; ++i) {
        __m512i diff = _mm512_sub_epi64(a[i], b[i]);
        if (borrow) diff = _mm512_sub_epi64(diff, _mm512_set1_epi64(1));
        
        __mmask8 b1 = _mm512_cmputail_epu64_mask(a[i], b[i]);
        __mmask8 b2 = _mm512_cmpeq_epu64_mask(a[i], b[i]);
        borrow = b1 | (b2 & borrow);
        r[i] = diff;
    }
}

inline void add_mod_avx512(fe_avx512& r, const fe_avx512& a, const fe_avx512& b) {
    add_avx512(r, a, b);
}

inline void sub_mod_avx512(fe_avx512& r, const fe_avx512& a, const fe_avx512& b) {
    fe_avx512 p_limbs = {
        _mm512_set1_epi64(0xFFFFFFFFFFFFFF43ULL),
        _mm512_set1_epi64(0xFFFFFFFFFFFFFFFFULL),
        _mm512_set1_epi64(0xFFFFFFFFFFFFFFFFULL),
        _mm512_set1_epi64(0xFFFFFFFEFFFFFFFFULL)
    };
    fe_avx512 t;
    add_avx512(t, a, p_limbs);
    sub_avx512(r, t, b);
}

inline void mul_mod_avx512(fe_avx512& r, const fe_avx512& a, const fe_avx512& b) {
    __m512i acc[8];
    for(int i=0; i<8; ++i) acc[i] = _mm512_setzero_si512();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            __m512i lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            __m512i old_acc = acc[i+j];
            acc[i+j] = _mm512_add_epi64(acc[i+j], lo);
            __mmask8 carry = _mm512_cmputail_epu64_mask(acc[i+j], old_acc);
            
            __m512i hi_plus_carry = _mm512_mask_add_epi64(hi, carry, hi, _mm512_set1_epi64(1));
            int k = i + j + 1;
            while (k < 8) {
                __m512i old_k = acc[k];
                acc[k] = _mm512_add_epi64(acc[k], hi_plus_carry);
                __mmask8 ck = _mm512_cmputail_epu64_mask(acc[k], old_k);
                if (ck == 0) break;
                hi_plus_carry = _mm512_set1_epi64(1);
                k++;
            }
        }
    }
    
    // Reduction
    __m512i c_val = _mm512_set1_epi64(977);
    for (int i = 0; i < 4; ++i) {
        __m512i h = acc[i+4];
        __m512i p_lo, p_hi;
        mul64x64_full(h, c_val, p_lo, p_hi);
        __m512i s_lo = _mm512_slli_epi64(h, 32);
        __m512i s_hi = _mm512_srli_epi64(h, 32);
        
        fe_avx512 to_add = { _mm512_setzero_si512() };
        to_add[0] = _mm512_add_epi64(p_lo, s_lo);
        __mmask8 c0 = _mm512_cmputail_epu64_mask(to_add[0], p_lo);
        to_add[1] = _mm512_mask_add_epi64(_mm512_add_epi64(p_hi, s_hi), c0, _mm512_add_epi64(p_hi, s_hi), _mm512_set1_epi64(1));
        
        fe_avx512 current_acc;
        for(int l=0; l<4; ++l) current_acc[l] = acc[l];
        add_avx512(current_acc, current_acc, to_add);
        for(int l=0; l<4; ++l) acc[l] = current_acc[l];
    }

    for(int i=0; i<4; ++i) r[i] = acc[i];
}

inline void square_mod_avx512(fe_avx512& r, const fe_avx512& a) {
    mul_mod_avx512(r, a, a);
}

inline ProjectivePoint point_add_avx512(ProjectivePoint p1, ProjectivePoint p2) {
    ProjectivePoint res;
    fe_avx512 z1_2, z2_2, u1, u2, s1, s2, h, r_val, h2, h3, u1h2;

    square_mod_avx512(z1_2, p1.z);
    square_mod_avx512(z2_2, p2.z);
    mul_mod_avx512(u1, p1.x, z2_2);
    mul_mod_avx512(u2, p2.x, z1_2);

    fe_avx512 z1_3, z2_3;
    mul_mod_avx512(z1_3, z1_2, p1.z);
    mul_mod_avx512(z2_3, z2_2, p2.z);
    mul_mod_avx512(s1, p1.y, z2_3);
    mul_mod_avx512(s2, p2.y, z1_3);

    sub_mod_avx512(h, u2, u1);
    sub_mod_avx512(r_val, s2, s1);

    fe_avx512 z1z2;
    mul_mod_avx512(z1z2, p1.z, p2.z);
    mul_mod_avx512(res.z, z1z2, h);

    square_mod_avx512(h2, h);
    mul_mod_avx512(h3, h2, h);
    mul_mod_avx512(u1h2, u1, h2);
    
    fe_avx512 r2, u1h2_2;
    square_mod_avx512(r2, r_val);
    add_mod_avx512(u1h2_2, u1h2, u1h2);
    
    sub_mod_avx512(res.x, r2, h3);
    sub_mod_avx512(res.x, res.x, u1h2_2);

    fe_avx512 u1h2_x3, r_u1h2_x3, s1h3;
    sub_mod_avx512(u1h2_x3, u1h2, res.x);
    mul_mod_avx512(r_u1h2_x3, r_val, u1h2_x3);
    mul_mod_avx512(s1h3, s1, h3);
    sub_mod_avx512(res.y, r_u1h2_x3, s1h3);

    return res;
}

}  // namespace bchaves::core::secp256k1_avx512

#endif  // defined(__AVX512F__)