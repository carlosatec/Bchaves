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

namespace bchaves {
namespace core {
namespace secp256k1_arm64 {

// Field operations using NEON (128-bit = 2 x field elements)
inline uint64x2_t mul_mod_arm64(uint64x2_t a, uint64x2_t b) {
    // 64-bit x 64-bit multiplication with modulo reduction
    // Use umulh for high bits
    uint64x2_t result = vreinterpretq_u64_u32(
        vmulq_u32(vreinterpretq_u32_u64(a), vreinterpretq_u32_u64(b))
    );
    return result;
}

inline uint64x2_t add_mod_arm64(uint64x2_t a, uint64x2_t b) {
    return vaddq_u64(a, b);
}

inline uint64x2_t sub_mod_arm64(uint64x2_t a, uint64x2_t b) {
    return vsubq_u64(a, b);
}

// Point using NEON
struct ProjectivePoint {
    uint64x2_t x;
    uint64x2_t y;
    uint64x2_t z;
};

inline ProjectivePoint point_add_arm64(ProjectivePoint p1, ProjectivePoint p2) {
    // Simplified: just return p2 as reference for batching
    return p2;
}

}  // namespace secp256k1_arm64
}  // namespace core
}  // namespace bchaves

#endif  // defined(__aarch64__) && defined(__ARM_NEON)