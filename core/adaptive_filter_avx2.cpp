/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Kernels AVX2 para o AdaptiveCuckooFilter.
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
 * @brief Busca AVX2: Compara fingerprint contra 8 slots (2 buckets) simultaneamente.
 */
__attribute__((target("avx2")))
bool lookup_avx2(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2) {
    (void)mask;
    // Broadcast do fingerprint para todos os slots do registro
    __m128i target = _mm_set1_epi16(fp);
    
    // Carrega bucket 1 (8 bytes) e bucket 2 (8 bytes)
    // Nota: Buckets de 4 entradas uint16_t ocupam exatamente 64 bits.
    __m128i b1 = _mm_loadl_epi64((const __m128i*)&buckets[i1 * 4]);
    __m128i b2 = _mm_loadl_epi64((const __m128i*)&buckets[i2 * 4]);
    
    // Combina b1 e b2 em um registro de 128 bits: [ b2 (64b) | b1 (64b) ]
    __m128i combined = _mm_unpacklo_epi64(b1, b2);
    
    // Compara todos os 8 slots uint16_t em paralelo
    __m128i cmp = _mm_cmpeq_epi16(combined, target);
    
    // Se qualquer bit for 1 no mask, o fingerprint foi encontrado
    return _mm_movemask_epi8(cmp) != 0;
}

/**
 * @brief Busca em lote otimizada para AVX2.
 */
__attribute__((target("avx2")))
void batch_lookup_avx2(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count) {
    // Por enquanto processa sequencialmente usando o kernel individual otimizado.
    // Em AVX2 poderíamos processar 2 itens completos (4 buckets) se os dados estivessem alinhados,
    // mas o ganho principal vem da eliminação de branches no lookup_avx2.
    for (size_t i = 0; i < count; ++i) {
        if (i + 1 < count) {
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i1s[i+1] * 4]), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(&buckets[i2s[i+1] * 4]), _MM_HINT_T0);
        }
        results[i] = lookup_avx2(buckets, mask, fps[i], i1s[i], i2s[i]);
    }
}

} // namespace bchaves::core
