/*
 * Bchaves: Unified SIMD Fleet Dispatcher
 * 
 * Descrição: Interface comum para operações vetoriais (Fleet) 
 *            em diferentes arquiteturas.
 */
#pragma once

#include <cstdio>
#include "core/secp256k1.hpp"
#include "core/secp256k1-sse4.hpp"
#include "core/secp256k1-avx2.hpp"
#include "core/secp256k1-avx512.hpp"
#include "core/secp256k1-arm64.hpp"
#include "system/hardware.hpp"

namespace bchaves::core::fleet {

enum class BackendType {
    SCALAR = 1,
    SSE4   = 2,
    AVX2   = 4,
    AVX512 = 8,
    ARM64  = 2
};

struct FleetDispatcher {
    BackendType active_backend;
    size_t lanes;

    FleetDispatcher() {
        // Detecção segura
        auto hw = bchaves::system::detect_hardware();
        if (hw.features & bchaves::system::cpu_avx512) {
            active_backend = BackendType::AVX512;
            lanes = 8;
        } else if (hw.features & bchaves::system::cpu_avx2) {
            active_backend = BackendType::AVX2;
            lanes = 4;
        } else if (hw.features & bchaves::system::cpu_sse4) {
            active_backend = BackendType::SSE4;
            lanes = 2;
        } else {
            active_backend = BackendType::SCALAR;
            lanes = 1;
        }
    }
};

// Meyers Singleton para evitar crash em inicialização estática
inline FleetDispatcher& get_dispatcher() {
    static FleetDispatcher instance;
    return instance;
}

/**
 * Pack/Unpack Templates e Helpers
 */

inline void pack_sse4(bchaves::core::secp256k1_sse4::ProjectivePoint& dst, const bchaves::core::PointJacobian* src) {
#if defined(__SSE4_1__)
    for (int i = 0; i < 4; ++i) {
        dst.x[i] = _mm_set_epi64x(src[1].x.limbs[i], src[0].x.limbs[i]);
        dst.y[i] = _mm_set_epi64x(src[1].y.limbs[i], src[0].y.limbs[i]);
        dst.z[i] = _mm_set_epi64x(src[1].z.limbs[i], src[0].z.limbs[i]);
    }
#endif
}

inline void unpack_sse4(bchaves::core::PointJacobian* dst, const bchaves::core::secp256k1_sse4::ProjectivePoint& src) {
#if defined(__SSE4_1__)
    uint64_t x[2], y[2], z[2];
    for (int i = 0; i < 4; ++i) {
        _mm_storeu_si128((__m128i*)x, src.x[i]);
        _mm_storeu_si128((__m128i*)y, src.y[i]);
        _mm_storeu_si128((__m128i*)z, src.z[i]);
        for(int l=0; l<2; ++l) {
            dst[l].x.limbs[i] = x[l];
            dst[l].y.limbs[i] = y[l];
            dst[l].z.limbs[i] = z[l];
        }
    }
#endif
}

inline void pack_avx2(bchaves::core::secp256k1_avx2::ProjectivePoint& dst, const bchaves::core::PointJacobian* src) {
#if defined(__AVX2__)
    for (int i = 0; i < 4; ++i) {
        dst.x[i] = _mm256_set_epi64x(src[3].x.limbs[i], src[2].x.limbs[i], src[1].x.limbs[i], src[0].x.limbs[i]);
        dst.y[i] = _mm256_set_epi64x(src[3].y.limbs[i], src[2].y.limbs[i], src[1].y.limbs[i], src[0].y.limbs[i]);
        dst.z[i] = _mm256_set_epi64x(src[3].z.limbs[i], src[2].z.limbs[i], src[1].z.limbs[i], src[0].z.limbs[i]);
    }
#endif
}

inline void unpack_avx2(bchaves::core::PointJacobian* dst, const bchaves::core::secp256k1_avx2::ProjectivePoint& src) {
#if defined(__AVX2__)
    uint64_t x[4], y[4], z[4];
    for (int i = 0; i < 4; ++i) {
        _mm256_storeu_si256((__m256i*)x, src.x[i]);
        _mm256_storeu_si256((__m256i*)y, src.y[i]);
        _mm256_storeu_si256((__m256i*)z, src.z[i]);
        for(int l=0; l<4; ++l) {
            dst[l].x.limbs[i] = x[l];
            dst[l].y.limbs[i] = y[l];
            dst[l].z.limbs[i] = z[l];
        }
    }
#endif
}

} // namespace bchaves::core::fleet
