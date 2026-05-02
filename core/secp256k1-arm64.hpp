/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Secp256k1 elliptic curve arithmetic optimized for ARM64 NEON.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__aarch64__) && defined(__ARM_NEON)

#include <arm_neon.h>
#include <cstdint>
#include <cstddef>
#include "core/secp256k1_reduce.hpp"

namespace bchaves::core::secp256k1_arm64 {

typedef uint64x2_t fe_arm64[4];

struct ProjectivePoint {
    fe_arm64 x;
    fe_arm64 y;
    fe_arm64 z;
};

inline void mul64x64_full(uint64x2_t a, uint64x2_t b, uint64x2_t& r_lo, uint64x2_t& r_hi) {
    uint32x2_t a32 = vmovn_u64(a);
    uint32x2_t b32 = vmovn_u64(b);
    uint32x2_t a32h = vshrn_n_u64(a, 32);
    uint32x2_t b32h = vshrn_n_u64(b, 32);

    uint64x2_t ll = vmull_u32(a32, b32);
    uint64x2_t lh = vmull_u32(a32, b32h);
    uint64x2_t hl = vmull_u32(a32h, b32);
    uint64x2_t hh = vmull_u32(a32h, b32h);

    uint64x2_t mid = vaddq_u64(lh, hl);
    uint64x2_t mid_lo = vshlq_n_u64(mid, 32);
    uint64x2_t mid_hi = vshrq_n_u64(mid, 32);

    r_lo = vaddq_u64(ll, mid_lo);
    r_hi = vaddq_u64(hh, mid_hi);
    
    uint64x2_t carry = vcltq_u64(r_lo, ll);
    r_hi = vaddq_u64(r_hi, vsubq_u64(vmovq_n_u64(0), carry));
}

inline void add_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t carry = vmovq_n_u64(0);
    for (int i = 0; i < 4; ++i) {
        uint64x2_t sum = vaddq_u64(a[i], b[i]);
        sum = vaddq_u64(sum, carry);
        uint64x2_t c1 = vcltq_u64(sum, a[i]);
        uint64x2_t c2 = vceqq_u64(sum, a[i]);
        carry = vshrq_n_u64(vorrq_u64(c1, vandq_u64(c2, vcltq_u64(vmovq_n_u64(0), carry))), 63);
        r[i] = sum;
    }
}

inline void sub_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t borrow = vmovq_n_u64(0);
    for (int i = 0; i < 4; ++i) {
        uint64x2_t diff = vsubq_u64(a[i], b[i]);
        diff = vsubq_u64(diff, borrow);
        uint64x2_t c1 = vcltq_u64(a[i], b[i]);
        uint64x2_t c2 = vceqq_u64(a[i], b[i]);
        borrow = vshrq_n_u64(vorrq_u64(c1, vandq_u64(c2, vcltq_u64(vmovq_n_u64(0), borrow))), 63);
        r[i] = diff;
    }
}

inline void add_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    add_arm64(r, a, b);
}

inline void sub_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    fe_arm64 p_limbs = {
        vmovq_n_u64(0xFFFFFFFFFFFFFF43ULL),
        vmovq_n_u64(0xFFFFFFFFFFFFFFFFULL),
        vmovq_n_u64(0xFFFFFFFFFFFFFFFFULL),
        vmovq_n_u64(0xFFFFFFFEFFFFFFFFULL)
    };
    fe_arm64 t;
    add_arm64(t, a, p_limbs);
    sub_arm64(r, t, b);
}

inline void mul_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t acc[8] = { vmovq_n_u64(0) };
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            uint64x2_t lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            uint64x2_t old_acc = acc[i+j];
            acc[i+j] = vaddq_u64(acc[i+j], lo);
            uint64x2_t carry = vshrq_n_u64(vcltq_u64(acc[i+j], old_acc), 63);
            
            uint64x2_t hi_plus_carry = vaddq_u64(hi, carry);
            int k = i + j + 1;
            while (k < 8) {
                uint64x2_t old_k = acc[k];
                acc[k] = vaddq_u64(acc[k], hi_plus_carry);
                uint64x2_t ck = vcltq_u64(acc[k], old_k);
                if (vgetq_lane_u64(vorrq_u64(ck, vmovq_n_u64(0)), 0) == 0) break;
                hi_plus_carry = vmovq_n_u64(1);
                k++;
            }
        }
    }
    
    // Reduction
    uint64x2_t c_val = vmovq_n_u64(977);
    for (int i = 0; i < 4; ++i) {
        uint64x2_t h = acc[i+4];
        uint64x2_t p_lo, p_hi;
        mul64x64_full(h, c_val, p_lo, p_hi);
        uint64x2_t s_lo = vshlq_n_u64(h, 32);
        uint64x2_t s_hi = vshrq_n_u64(h, 32);
        
        fe_arm64 to_add = { vmovq_n_u64(0) };
        to_add[0] = vaddq_u64(p_lo, s_lo);
        uint64x2_t c0 = vshrq_n_u64(vcltq_u64(to_add[0], p_lo), 63);
        to_add[1] = vaddq_u64(vaddq_u64(p_hi, s_hi), c0);
        
        fe_arm64 current_acc;
        for(int l=0; l<4; ++l) current_acc[l] = acc[l];
        add_arm64(current_acc, current_acc, to_add);
        for(int l=0; l<4; ++l) acc[l] = current_acc[l];
    }

    for(int i=0; i<4; ++i) r[i] = acc[i];
}

inline void square_mod_arm64(fe_arm64& r, const fe_arm64& a) {
    mul_mod_arm64(r, a, a);
}

inline ProjectivePoint point_add_arm64(ProjectivePoint p1, ProjectivePoint p2) {
    ProjectivePoint res;
    fe_arm64 z1_2, z2_2, u1, u2, s1, s2, h, r_val, h2, h3, u1h2;

    square_mod_arm64(z1_2, p1.z);
    square_mod_arm64(z2_2, p2.z);
    mul_mod_arm64(u1, p1.x, z2_2);
    mul_mod_arm64(u2, p2.x, z1_2);

    fe_arm64 z1_3, z2_3;
    mul_mod_arm64(z1_3, z1_2, p1.z);
    mul_mod_arm64(z2_3, z2_2, p2.z);
    mul_mod_arm64(s1, p1.y, z2_3);
    mul_mod_arm64(s2, p2.y, z1_3);

    sub_mod_arm64(h, u2, u1);
    sub_mod_arm64(r_val, s2, s1);

    fe_arm64 z1z2;
    mul_mod_arm64(z1z2, p1.z, p2.z);
    mul_mod_arm64(res.z, z1z2, h);

    square_mod_arm64(h2, h);
    mul_mod_arm64(h3, h2, h);
    mul_mod_arm64(u1h2, u1, h2);
    
    fe_arm64 r2, u1h2_2;
    square_mod_arm64(r2, r_val);
    add_mod_arm64(u1h2_2, u1h2, u1h2);
    
    sub_mod_arm64(res.x, r2, h3);
    sub_mod_arm64(res.x, res.x, u1h2_2);

    fe_arm64 u1h2_x3, r_u1h2_x3, s1h3;
    sub_mod_arm64(u1h2_x3, u1h2, res.x);
    mul_mod_arm64(r_u1h2_x3, r_val, u1h2_x3);
    mul_mod_arm64(s1h3, s1, h3);
    sub_mod_arm64(res.y, r_u1h2_x3, s1h3);

    return res;
}

}  // namespace bchaves::core::secp256k1_arm64

#endif  // defined(__aarch64__) && defined(__ARM_NEON)