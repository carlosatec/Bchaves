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

namespace bchaves::core::secp256k1_arm64 {

/**
 * @brief Elemento de campo representado em 4 registros uint64x2_t (cada registro guarda 2 lanes).
 * Isso permite processar 2 field elements em paralelo.
 */
typedef uint64x2_t fe_arm64[4];

struct ProjectivePoint {
    fe_arm64 x;
    fe_arm64 y;
    fe_arm64 z;
};

/**
 * @brief Multiplicação 64x64 -> 128-bit usando NEON.
 */
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
    r_hi = vaddq_u64(r_hi, vnegq_s64(vreinterpretq_s64_u64(carry)));
}

/**
 * @brief Adição 256-bit unrolled.
 */
inline void add_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t s0 = vaddq_u64(a[0], b[0]);
    uint64x2_t c0 = vshrq_n_u64(vcltq_u64(s0, a[0]), 63);
    
    uint64x2_t s1 = vaddq_u64(a[1], b[1]);
    s1 = vaddq_u64(s1, c0);
    uint64x2_t c1 = vshrq_n_u64(vorrq_u64(vcltq_u64(s1, a[1]), vandq_u64(vceqq_u64(s1, a[1]), c0)), 63);
    
    uint64x2_t s2 = vaddq_u64(a[2], b[2]);
    s2 = vaddq_u64(s2, c1);
    uint64x2_t c2 = vshrq_n_u64(vorrq_u64(vcltq_u64(s2, a[2]), vandq_u64(vceqq_u64(s2, a[2]), c1)), 63);
    
    uint64x2_t s3 = vaddq_u64(a[3], b[3]);
    s3 = vaddq_u64(s3, c2);
    
    r[0] = s0; r[1] = s1; r[2] = s2; r[3] = s3;
}

/**
 * @brief Subtração 256-bit unrolled.
 */
inline void sub_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t d0 = vsubq_u64(a[0], b[0]);
    uint64x2_t b0 = vshrq_n_u64(vcltq_u64(a[0], b[0]), 63);
    
    uint64x2_t d1 = vsubq_u64(a[1], b[1]);
    d1 = vsubq_u64(d1, b0);
    uint64x2_t b1 = vshrq_n_u64(vorrq_u64(vcltq_u64(a[1], b[1]), vandq_u64(vceqq_u64(a[1], b[1]), b0)), 63);
    
    uint64x2_t d2 = vsubq_u64(a[2], b[2]);
    d2 = vsubq_u64(d2, b1);
    uint64x2_t b2 = vshrq_n_u64(vorrq_u64(vcltq_u64(a[2], b[2]), vandq_u64(vceqq_u64(a[2], b[2]), b1)), 63);
    
    uint64x2_t d3 = vsubq_u64(a[3], b[3]);
    d3 = vsubq_u64(d3, b2);
    
    r[0] = d0; r[1] = d1; r[2] = d2; r[3] = d3;
}

inline void add_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    add_arm64(r, a, b);
}

inline void sub_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    static const fe_arm64 p_limbs = {
        vdupq_n_u64(0xFFFFFFFFFFFFFF43ULL),
        vdupq_n_u64(0xFFFFFFFFFFFFFFFFULL),
        vdupq_n_u64(0xFFFFFFFFFFFFFFFFULL),
        vdupq_n_u64(0xFFFFFFFEFFFFFFFFULL)
    };
    fe_arm64 t;
    add_arm64(t, a, p_limbs);
    sub_arm64(r, t, b);
}

/**
 * @brief Multiplicação modular Secp256k1 otimizada para NEON.
 */
inline void mul_mod_arm64(fe_arm64& r, const fe_arm64& a, const fe_arm64& b) {
    uint64x2_t acc[8] = { vdupq_n_u64(0) };
    
    // Unrolled 4x4 multiplication
    for (int i = 0; i < 4; ++i) {
        uint64x2_t carry = vdupq_n_u64(0);
        for (int j = 0; j < 4; ++j) {
            uint64x2_t lo, hi;
            mul64x64_full(a[i], b[j], lo, hi);
            
            uint64x2_t old_acc = acc[i+j];
            acc[i+j] = vaddq_u64(vaddq_u64(acc[i+j], lo), carry);
            carry = vaddq_u64(hi, vshrq_n_u64(vcltq_u64(acc[i+j], old_acc), 63));
        }
        acc[i+4] = carry;
    }
    
    // Fast reduction for Secp256k1
    // p = 2^256 - 2^32 - 977
    uint64x2_t c_val = vdupq_n_u64(977);
    for (int i = 0; i < 4; ++i) {
        uint64x2_t h = acc[i+4];
        uint64x2_t p_lo, p_hi;
        mul64x64_full(h, c_val, p_lo, p_hi);
        uint64x2_t s_lo = vshlq_n_u64(h, 32);
        uint64x2_t s_hi = vshrq_n_u64(h, 32);
        
        fe_arm64 to_add = { vdupq_n_u64(0) };
        to_add[0] = vaddq_u64(p_lo, s_lo);
        uint64x2_t carry0 = vshrq_n_u64(vcltq_u64(to_add[0], p_lo), 63);
        to_add[1] = vaddq_u64(vaddq_u64(p_hi, s_hi), carry0);
        
        add_arm64(acc, acc, to_add);
    }

    r[0] = acc[0]; r[1] = acc[1]; r[2] = acc[2]; r[3] = acc[3];
}

inline void square_mod_arm64(fe_arm64& r, const fe_arm64& a) {
    mul_mod_arm64(r, a, a);
}

inline ProjectivePoint point_add_arm64(const ProjectivePoint& p1, const ProjectivePoint& p2) {
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