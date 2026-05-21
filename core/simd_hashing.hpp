#pragma once

#include <cstdint>
#include <cstring>

#if defined(__AVX2__) || defined(__SSSE3__)
#include <immintrin.h>
#endif

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace bchaves::core {

/**
 * Utilitários SIMD para extração de fingerprints e índices do Cuckoo Filter.
 * Otimizado para processar 8 hashes RIPEMD-160 simultaneamente (AVX2).
 */
struct SimdCuckooHashes {
    alignas(32) uint16_t fingerprints[8];
    alignas(32) uint64_t indexes[8];
};

/**
 * Calcula fingerprints e índices primários para 8 hashes de 20 bytes.
 * Assume que os hashes estão no layout retornado por ripemd160_batch8.
 */
inline void compute_cuckoo_hashes_batch8(const uint8_t* const hashes[8], SimdCuckooHashes& out, uint64_t bucket_mask) {
    // Processamos as palavras de 32 bits diretamente para evitar bswap se possível,
    // mas o RIPEMD-160 padrão Bitcoin/Litecoin é little-endian no output.
    
    for (int i = 0; i < 8; ++i) {
        const uint32_t* h = reinterpret_cast<const uint32_t*>(hashes[i]);
        
        // Fingerprint: primeiros 16 bits
        uint16_t fp = static_cast<uint16_t>(h[0] & 0xFFFF);
        out.fingerprints[i] = (fp == 0) ? 1 : fp;
        
        // Index: 64 bits de entropia combinando h[0](high), h[1] e h[2](low)
        uint64_t idx = (static_cast<uint64_t>(h[0] >> 16)) |
                       (static_cast<uint64_t>(h[1]) << 16) |
                       (static_cast<uint64_t>(h[2] & 0xFFFF) << 48);
        
        out.indexes[i] = idx & bucket_mask;
    }
}

/**
 * Serializa quatro limbs little-endian de 64 bits em 32 bytes big-endian.
 * O fallback escalar preserva portabilidade; os caminhos SIMD fazem o mesmo
 * byte-shuffle sem chamadas repetidas de bswap no hot path.
 */
inline void store_bigint_be32(const uint64_t* limbs, uint8_t* out) {
#if defined(__AVX2__)
    const __m256i v = _mm256_set_epi64x(
        static_cast<long long>(limbs[0]),
        static_cast<long long>(limbs[1]),
        static_cast<long long>(limbs[2]),
        static_cast<long long>(limbs[3]));
    const __m256i shuf = _mm256_set_epi8(
        24, 25, 26, 27, 28, 29, 30, 31,
        16, 17, 18, 19, 20, 21, 22, 23,
         8,  9, 10, 11, 12, 13, 14, 15,
         0,  1,  2,  3,  4,  5,  6,  7);
    const __m256i be = _mm256_shuffle_epi8(v, shuf);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), be);
#elif defined(__SSSE3__)
    const __m128i hi = _mm_set_epi64x(
        static_cast<long long>(limbs[2]),
        static_cast<long long>(limbs[3]));
    const __m128i lo = _mm_set_epi64x(
        static_cast<long long>(limbs[0]),
        static_cast<long long>(limbs[1]));
    const __m128i shuf = _mm_set_epi8(
        8, 9, 10, 11, 12, 13, 14, 15,
        0, 1, 2, 3, 4, 5, 6, 7);
    const __m128i hi_be = _mm_shuffle_epi8(hi, shuf);
    const __m128i lo_be = _mm_shuffle_epi8(lo, shuf);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out), hi_be);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 16), lo_be);
#elif defined(__aarch64__)
    const uint64x2_t hi = {limbs[3], limbs[2]};
    const uint64x2_t lo = {limbs[1], limbs[0]};
    const uint8x16_t hi_be = vrev64q_u8(vreinterpretq_u8_u64(hi));
    const uint8x16_t lo_be = vrev64q_u8(vreinterpretq_u8_u64(lo));
    vst1q_u8(out, hi_be);
    vst1q_u8(out + 16, lo_be);
#else
    const uint64_t swapped[4] = {
        __builtin_bswap64(limbs[3]),
        __builtin_bswap64(limbs[2]),
        __builtin_bswap64(limbs[1]),
        __builtin_bswap64(limbs[0])
    };
    std::memcpy(out, swapped, 32);
#endif
}

/**
 * Versão AVX2 para busca em lote (Experimental/Futuro)
 * Por enquanto, a versão loop-unrolled acima é muito eficiente pois o gargalo é o acesso à RAM.
 */
} // namespace bchaves::core
