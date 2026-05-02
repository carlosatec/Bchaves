#pragma once

#include <immintrin.h>
#include <cstdint>

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
 * Versão AVX2 para busca em lote (Experimental/Futuro)
 * Por enquanto, a versão loop-unrolled acima é muito eficiente pois o gargalo é o acesso à RAM.
 */
} // namespace bchaves::core
