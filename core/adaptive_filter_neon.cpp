/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Kernels ARM64 NEON para o AdaptiveCuckooFilter.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <cstdint>
#include <cstddef>

#if defined(__aarch64__) || defined(__arm__)
#include <arm_neon.h>

namespace bchaves::core {

/**
 * @brief Busca NEON: Compara fingerprint contra 8 slots usando registros de 128 bits.
 */
bool lookup_neon(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2) {
    (void)mask;
    // Broadcast do fingerprint
    uint16x8_t target = vdupq_n_u16(fp);
    
    // Carrega bucket 1 (8 bytes) e bucket 2 (8 bytes)
    uint64x1_t b1 = vld1_u64((const uint64_t*)&buckets[i1 * 4]);
    uint64x1_t b2 = vld1_u64((const uint64_t*)&buckets[i2 * 4]);
    
    // Combina em um registro Q (128 bits)
    uint16x8_t combined = vreinterpretq_u16_u64(vcombine_u64(b1, b2));
    
    // Compara todos os slots
    uint16x8_t cmp = vceqq_u16(combined, target);
    
    // Redução: Verifica se qualquer lane é diferente de zero
#if defined(__aarch64__)
    return vmaxvq_u16(cmp) != 0;
#else
    // Fallback para ARMv7 
    uint16x4_t res = vorr_u16(vget_low_u16(cmp), vget_high_u16(cmp));
    return vget_lane_u16(vpmax_u16(res, res), 0) != 0;
#endif
}

/**
 * @brief Busca em lote otimizada para NEON.
 */
void batch_lookup_neon(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (i + 1 < count) {
            __builtin_prefetch(&buckets[i1s[i+1] * 4], 0, 0); // read, low locality
            __builtin_prefetch(&buckets[i2s[i+1] * 4], 0, 0);
        }
        results[i] = lookup_neon(buckets, mask, fps[i], i1s[i], i2s[i]);
    }
}

} // namespace bchaves::core

#endif
