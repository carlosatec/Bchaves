/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Kernels AVX-512 para o AdaptiveCuckooFilter.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <immintrin.h>
#include <cstdint>
#include <cstddef>

namespace bchaves::core {

/**
 * @brief Busca AVX-512: Utiliza máscaras nativas para comparação ultra-rápida.
 */
__attribute__((target("avx512vl,avx512bw,avx512f")))
bool lookup_avx512(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2) {
    (void)mask;
    // Broadcast do fingerprint
    __m128i target = _mm_set1_epi16(fp);
    
    // Carrega buckets (8 bytes cada)
    __m128i b1 = _mm_loadl_epi64((const __m128i*)&buckets[i1 * 4]);
    __m128i b2 = _mm_loadl_epi64((const __m128i*)&buckets[i2 * 4]);
    
    // Combina em 128 bits
    __m128i combined = _mm_unpacklo_epi64(b1, b2);
    
    // Compara usando AVX-512VL (Variable Length) que retorna uma máscara de bits
    // Isso é mais rápido que o movemask do AVX2.
    __mmask8 result = _mm_cmpeq_epi16_mask(combined, target);
    
    return result != 0;
}

/**
 * @brief Busca em lote otimizada para AVX-512.
 */
__attribute__((target("avx512vl,avx512bw,avx512f")))
void batch_lookup_avx512(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (i + 1 < count) {
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i1s[i+1] * 4]), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i2s[i+1] * 4]), _MM_HINT_T0);
        }
        results[i] = lookup_avx512(buckets, mask, fps[i], i1s[i], i2s[i]);
    }
}

} // namespace bchaves::core
