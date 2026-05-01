/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: SHA-256 implementation for AVX (128-bit, not AVX2).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__AVX__) && !defined(__AVX2__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>

namespace bchaves {
namespace core {
namespace sha256_avx {

// AVX 128-bit uses similar approach to SSE4 but with 128-bit registers
// Falls back to SSE4 implementation when AVX2 not available

}  // namespace sha256_avx
}  // namespace core
}  // namespace bchaves

#endif  // defined(__AVX__) && !defined(__AVX2__)