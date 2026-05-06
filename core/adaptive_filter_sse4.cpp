/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Kernels SSE4 para o AdaptiveCuckooFilter.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <smmintrin.h>
#include <cstdint>
#include <cstddef>

namespace bchaves::core {

/**
 * @brief Busca SSE4: Compara fingerprint contra 8 slots (2 buckets) em 128 bits.
 */
__attribute__((target("sse4.2")))
bool lookup_sse4(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2) {
    (void)mask;
    // Broadcast do fingerprint para 128 bits
    __m128i target = _mm_set1_epi16(fp);
    
    // Carrega bucket 1 (8 bytes) e bucket 2 (8 bytes)
    __m128i b1 = _mm_loadl_epi64((const __m128i*)&buckets[i1 * 4]);
    __m128i b2 = _mm_loadl_epi64((const __m128i*)&buckets[i2 * 4]);
    
    // Combina: [ b2 | b1 ] (Total 128 bits)
    __m128i combined = _mm_unpacklo_epi64(b1, b2);
    
    // Compara os 8 slots em paralelo
    __m128i cmp = _mm_cmpeq_epi16(combined, target);
    
    return _mm_movemask_epi8(cmp) != 0;
}

/**
 * @brief Busca em lote otimizada para SSE4.
 */
__attribute__((target("sse4.2")))
void batch_lookup_sse4(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (i + 1 < count) {
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i1s[i+1] * 4]), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i2s[i+1] * 4]), _MM_HINT_T0);
        }
        results[i] = lookup_sse4(buckets, mask, fps[i], i1s[i], i2s[i]);
    }
}

} // namespace bchaves::core
