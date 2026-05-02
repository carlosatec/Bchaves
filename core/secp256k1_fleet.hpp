/*
 * Bchaves: Unified SIMD Fleet Dispatcher
 * 
 * Descrição: Interface comum para operações vetoriais (Fleet) 
 *            em diferentes arquiteturas.
 */
#pragma once

#include <cstdio>
#include <vector>
#include <cstring>
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

struct FleetState {
    size_t size;
    std::vector<uint64_t> x[4];
    std::vector<uint64_t> y[4];
    std::vector<uint64_t> z[4];
    std::vector<uint64_t> d[4]; // distances
    std::vector<bool> is_wild;
    std::vector<bchaves::core::BigInt> scratch; 

    FleetState(size_t n) : size(n) {
        for(int i=0; i<4; ++i) {
            x[i].resize(n);
            y[i].resize(n);
            z[i].resize(n);
            d[i].resize(n);
        }
        is_wild.resize(n, false);
        scratch.resize(n);
    }
};

struct FleetDispatcher {
    BackendType active_backend;
    size_t lanes;

    FleetDispatcher() {
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

inline FleetDispatcher& get_dispatcher() {
    static FleetDispatcher instance;
    return instance;
}

// ----------------------------------------------------------------------------
// SSE4 Backend Helpers (Direct Load/Store)
// ----------------------------------------------------------------------------
#if defined(__SSE4_1__)
inline void add_fleet_sse4(FleetState& state, size_t idx, const bchaves::core::Secp256k1Point& jump) {
    bchaves::core::secp256k1_sse4::ProjectivePoint p, j;
    for(int i=0; i<4; ++i) {
        p.x[i] = _mm_loadu_si128((const __m128i*)&state.x[i][idx]);
        p.y[i] = _mm_loadu_si128((const __m128i*)&state.y[i][idx]);
        p.z[i] = _mm_loadu_si128((const __m128i*)&state.z[i][idx]);
        j.x[i] = _mm_set1_epi64x(jump.x.limbs[i]);
        j.y[i] = _mm_set1_epi64x(jump.y.limbs[i]);
        j.z[i] = _mm_set1_epi64x(1);
    }
    auto res = bchaves::core::secp256k1_sse4::point_add_sse4(p, j);
    for(int i=0; i<4; ++i) {
        _mm_storeu_si128((__m128i*)&state.x[i][idx], res.x[i]);
        _mm_storeu_si128((__m128i*)&state.y[i][idx], res.y[i]);
        _mm_storeu_si128((__m128i*)&state.z[i][idx], res.z[i]);
    }
}
#endif

// ----------------------------------------------------------------------------
// AVX2 Backend Helpers (Direct Load/Store)
// ----------------------------------------------------------------------------
#if defined(__AVX2__)
inline void add_fleet_avx2(FleetState& state, size_t idx, const bchaves::core::Secp256k1Point& jump) {
    bchaves::core::secp256k1_avx2::ProjectivePoint p, j;
    for(int i=0; i<4; ++i) {
        p.x[i] = _mm256_loadu_si256((const __m256i*)&state.x[i][idx]);
        p.y[i] = _mm256_loadu_si256((const __m256i*)&state.y[i][idx]);
        p.z[i] = _mm256_loadu_si256((const __m256i*)&state.z[i][idx]);
        j.x[i] = _mm256_set1_epi64x(jump.x.limbs[i]);
        j.y[i] = _mm256_set1_epi64x(jump.y.limbs[i]);
        j.z[i] = _mm256_set1_epi64x(1);
    }
    auto res = bchaves::core::secp256k1_avx2::point_add_avx2(p, j);
    for(int i=0; i<4; ++i) {
        _mm256_storeu_si256((__m256i*)&state.x[i][idx], res.x[i]);
        _mm256_storeu_si256((__m256i*)&state.y[i][idx], res.y[i]);
        _mm256_storeu_si256((__m256i*)&state.z[i][idx], res.z[i]);
    }
}
#endif

// ----------------------------------------------------------------------------
// AVX512 Backend Helpers (Direct Load/Store)
// ----------------------------------------------------------------------------
#if defined(__AVX512F__)
inline void add_fleet_avx512(FleetState& state, size_t idx, const bchaves::core::Secp256k1Point& jump) {
    bchaves::core::secp256k1_avx512::ProjectivePoint p, j;
    for(int i=0; i<4; ++i) {
        p.x[i] = _mm512_loadu_si512((const __m512i*)&state.x[i][idx]);
        p.y[i] = _mm512_loadu_si512((const __m512i*)&state.y[i][idx]);
        p.z[i] = _mm512_loadu_si512((const __m512i*)&state.z[i][idx]);
        j.x[i] = _mm512_set1_epi64(jump.x.limbs[i]);
        j.y[i] = _mm512_set1_epi64(jump.y.limbs[i]);
        j.z[i] = _mm512_set1_epi64(1);
    }
    auto res = bchaves::core::secp256k1_avx512::point_add_avx512(p, j);
    for(int i=0; i<4; ++i) {
        _mm512_storeu_si512((__m512i*)&state.x[i][idx], res.x[i]);
        _mm512_storeu_si512((__m512i*)&state.y[i][idx], res.y[i]);
        _mm512_storeu_si512((__m512i*)&state.z[i][idx], res.z[i]);
    }
}
#endif

// ----------------------------------------------------------------------------
// ARM64 Backend Helpers (Direct Load/Store)
// ----------------------------------------------------------------------------
#if defined(__aarch64__) && defined(__ARM_NEON)
inline void add_fleet_arm64(FleetState& state, size_t idx, const bchaves::core::Secp256k1Point& jump) {
    bchaves::core::secp256k1_arm64::ProjectivePoint p, j;
    for(int i=0; i<4; ++i) {
        p.x[i] = vld1q_u64((const uint64_t*)&state.x[i][idx]);
        p.y[i] = vld1q_u64((const uint64_t*)&state.y[i][idx]);
        p.z[i] = vld1q_u64((const uint64_t*)&state.z[i][idx]);
        j.x[i] = vmovq_n_u64(jump.x.limbs[i]);
        j.y[i] = vmovq_n_u64(jump.y.limbs[i]);
        j.z[i] = vmovq_n_u64(1);
    }
    auto res = bchaves::core::secp256k1_arm64::point_add_arm64(p, j);
    for(int i=0; i<4; ++i) {
        vst1q_u64((uint64_t*)&state.x[i][idx], res.x[i]);
        vst1q_u64((uint64_t*)&state.y[i][idx], res.y[i]);
        vst1q_u64((uint64_t*)&state.z[i][idx], res.z[i]);
    }
}
#endif

/**
 * Normalização em lote SoA usando inversão de Montgomery.
 */
inline void batch_normalize_fleet(FleetState& state) {
    size_t n = state.size;
    std::vector<bchaves::core::BigInt> z_coords(n);
    for(size_t i=0; i<n; ++i) {
        for(int l=0; l<4; ++l) z_coords[i].limbs[l] = state.z[l][i];
    }
    
    bchaves::core::batch_mod_inv_k1(z_coords.data(), n, state.scratch.data());
    
    for(size_t i=0; i<n; ++i) {
        bchaves::core::BigInt zi = z_coords[i];
        bchaves::core::BigInt zi2 = bchaves::core::mod_mul_k1(zi, zi);
        bchaves::core::BigInt zi3 = bchaves::core::mod_mul_k1(zi2, zi);
        
        bchaves::core::BigInt xi, yi;
        for(int l=0; l<4; ++l) {
            xi.limbs[l] = state.x[l][i];
            yi.limbs[l] = state.y[l][i];
        }
        
        xi = bchaves::core::mod_mul_k1(xi, zi2);
        yi = bchaves::core::mod_mul_k1(yi, zi3);
        
        for(int l=0; l<4; ++l) {
            state.x[l][i] = xi.limbs[l];
            state.y[l][i] = yi.limbs[l];
            state.z[l][i] = (l == 0) ? 1 : 0; 
        }
    }
}

} // namespace bchaves::core::fleet
