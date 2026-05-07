/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação base do AdaptiveCuckooFilter.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "core/adaptive_filter.hpp"
#include "system/hardware.hpp"
#include <cstdlib>
#include <algorithm>

#if defined(_WIN32)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

namespace bchaves::core {

namespace {

// Kernels Escalares (Fallback)
bool lookup_scalar(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2) {
    (void)mask;
    const uint16_t* b1 = &buckets[i1 * 4];
    const uint16_t* b2 = &buckets[i2 * 4];
    
    if (b1[0] == fp || b1[1] == fp || b1[2] == fp || b1[3] == fp) return true;
    if (b2[0] == fp || b2[1] == fp || b2[2] == fp || b2[3] == fp) return true;
    
    return false;
}

void batch_lookup_scalar(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        results[i] = lookup_scalar(buckets, mask, fps[i], i1s[i], i2s[i]);
    }
}

} // namespace

AdaptiveCuckooFilter::AdaptiveCuckooFilter(size_t num_items, const system::HardwareInfo& hw) {
    // Calcula capacidade (potência de 2)
    size_t num_buckets = (num_items / kEntriesPerBucket) * 1.25; // 80% load factor
    m_capacity_pow2 = 1;
    while (m_capacity_pow2 < num_buckets) m_capacity_pow2 <<= 1;
    m_bucket_mask = m_capacity_pow2 - 1;
    
    // Alocação via HugePages (fallback transparente para alocação normal)
    // Adicionamos 64 bytes de padding para evitar SIMD read overflow no final do buffer
    size_t total_size = (m_capacity_pow2 * kBucketSize) + 64;
    m_alloc_size = total_size;
    m_buckets = static_cast<uint16_t*>(bchaves::system::allocate_huge_pages(total_size));
    if (!m_buckets) {
        // Fallback final: alocação convencional
#if defined(_WIN32)
        m_buckets = static_cast<uint16_t*>(_aligned_malloc(total_size, 64));
#else
        if (posix_memalign(reinterpret_cast<void**>(&m_buckets), 64, total_size) != 0) {
            m_buckets = nullptr;
        }
#endif
        m_use_huge_pages = false;
    } else {
        m_use_huge_pages = true;
    }
    
    if (m_buckets) {
        std::memset(m_buckets, 0, total_size);
    }
    
    m_count = 0;
    
    // Seleciona os kernels baseados no hardware
    select_kernels(hw);
}

AdaptiveCuckooFilter::~AdaptiveCuckooFilter() {
    if (m_buckets) {
        if (m_use_huge_pages) {
            bchaves::system::free_huge_pages(m_buckets, m_alloc_size);
        } else {
#if defined(_WIN32)
            _aligned_free(m_buckets);
#else
            free(m_buckets);
#endif
        }
    }
}

extern bool lookup_avx2(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2);
extern void batch_lookup_avx2(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count);

extern bool lookup_avx512(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2);
extern void batch_lookup_avx512(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count);

extern bool lookup_sse4(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2);
extern void batch_lookup_sse4(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count);

#if defined(__aarch64__) || defined(__arm__)
extern bool lookup_neon(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2);
extern void batch_lookup_neon(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count);
#endif

void AdaptiveCuckooFilter::select_kernels(const system::HardwareInfo& hw) {
    // Default Fallback
    m_kernel.lookup = lookup_scalar;
    m_kernel.batch_lookup = batch_lookup_scalar;

    // Despacho baseado nas features detectadas
    if (hw.features & system::cpu_avx512) {
        m_kernel.lookup = lookup_avx512;
        m_kernel.batch_lookup = batch_lookup_avx512;
    } else if (hw.features & system::cpu_avx2) {
        m_kernel.lookup = lookup_avx2;
        m_kernel.batch_lookup = batch_lookup_avx2;
    } else if (hw.features & system::cpu_sse4) {
        m_kernel.lookup = lookup_sse4;
        m_kernel.batch_lookup = batch_lookup_sse4;
    } else if (hw.features & system::cpu_neon) {
#if defined(__aarch64__) || defined(__arm__)
        m_kernel.lookup = lookup_neon;
        m_kernel.batch_lookup = batch_lookup_neon;
#endif
    }
}

bool AdaptiveCuckooFilter::insert(const uint8_t* hash160) {
    uint16_t fp = derive_fingerprint(hash160);
    size_t i1 = derive_index(hash160);
    
    if (insert_internal(fp, i1)) {
        m_count++;
        return true;
    }
    
    // Cuckoo Displacement usando um PRNG simples (Xorshift64) para ser rápido e determinístico por thread
    uint64_t rng = reinterpret_cast<uintptr_t>(hash160); 
    size_t cur_i = i1;
    for (size_t n = 0; n < kMaxCuckooCount; ++n) {
        // Xorshift64
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        
        size_t slot = rng % kEntriesPerBucket;
        uint16_t* bucket = &m_buckets[cur_i * kEntriesPerBucket];
        
        uint16_t old_fp = bucket[slot];
        bucket[slot] = fp;
        fp = old_fp;
        
        cur_i = alt_index(cur_i, fp);
        if (insert_internal(fp, cur_i)) {
            m_count++;
            return true;
        }
    }
    
    return false;
}

bool AdaptiveCuckooFilter::insert_internal(uint16_t fp, size_t i) {
    uint16_t* bucket = &m_buckets[i * kEntriesPerBucket];
    for (size_t s = 0; s < kEntriesPerBucket; ++s) {
        if (bucket[s] == 0) {
            bucket[s] = fp;
            return true;
        }
    }
    return false;
}

bool AdaptiveCuckooFilter::lookup(const uint8_t* hash160) const {
    uint16_t fp = derive_fingerprint(hash160);
    size_t i1 = derive_index(hash160);
    size_t i2 = alt_index(i1, fp);
    return m_kernel.lookup(m_buckets, m_bucket_mask, fp, i1, i2);
}

void AdaptiveCuckooFilter::lookup_batch(const uint8_t* hash160_batch, bool* results, size_t count) const {
    // Otimização: Para lotes pequenos (comum no Bchaves), usamos um buffer de stack menor
    // Para lotes grandes (até 1024), ainda usamos o stack mas com cautela.
    uint16_t fps[1024];
    size_t i1s[1024];
    size_t i2s[1024];
    
    size_t batch_size = std::min(count, (size_t)1024);
    for (size_t i = 0; i < batch_size; ++i) {
        const uint8_t* item = hash160_batch + (i * 20);
        fps[i] = derive_fingerprint(item);
        i1s[i] = derive_index(item);
        i2s[i] = alt_index(i1s[i], fps[i]);
    }
    
    m_kernel.batch_lookup(m_buckets, m_bucket_mask, fps, i1s, i2s, results, batch_size);
}

} // namespace bchaves::core
