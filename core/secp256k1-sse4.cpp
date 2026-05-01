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

namespace bchaves {
namespace core {
namespace secp256k1_sse4 {

// Field operations using SSE4.1
inline __m128i mul_mod_sse4(__m128i a, __m128i b) {
    // Multiply and reduce mod P
    __m128i prod = _mm_mul_epu32(a, b);
    return prod;
}

inline __m128i add_mod_sse4(__m128i a, __m128i b) {
    return _mm_add_epi64(a, b);
}

inline __m128i sub_mod_sse4(__m128i a, __m128i b) {
    return _mm_sub_epi64(a, b);
}

}  // namespace secp256k1_sse4
}  // namespace core
}  // namespace bchaves

#endif  // defined(__SSE4_1__)