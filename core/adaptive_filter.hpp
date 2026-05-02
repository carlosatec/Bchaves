/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Cuckoo Filter adaptativo com despacho SIMD e alinhamento de cache.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
#include <functional>
#include "system/types.hpp"

namespace bchaves::core {

/**
 * @brief Estrutura de despacho para kernels especializados de Cuckoo Filter.
 */
struct CuckooKernel {
    typedef bool (*LookupFunc)(const uint16_t* buckets, size_t mask, uint16_t fp, size_t i1, size_t i2);
    typedef void (*BatchLookupFunc)(const uint16_t* buckets, size_t mask, const uint16_t* fps, const size_t* i1s, const size_t* i2s, bool* results, size_t count);

    LookupFunc lookup;
    BatchLookupFunc batch_lookup;
};

/**
 * @brief Cuckoo Filter de alta performance que se adapta ao hardware (SIMD, Cache, NUMA).
 */
class AdaptiveCuckooFilter {
public:
    static constexpr size_t kEntriesPerBucket = 4;
    static constexpr size_t kMaxCuckooCount = 500;
    static constexpr size_t kBucketSize = kEntriesPerBucket * sizeof(uint16_t); // 8 bytes

    AdaptiveCuckooFilter(size_t num_items, const system::HardwareInfo& hw);
    ~AdaptiveCuckooFilter();

    // Inserção (Scalar, não crítico para throughput de busca)
    bool insert(const uint8_t* hash160);
    
    // Busca Individual
    bool lookup(const uint8_t* hash160) const;
    
    // Busca em Lote (SIMD-Optimized)
    void lookup_batch(const uint8_t* hash160_batch, bool* results, size_t count) const;

    size_t size() const { return m_count; }
    size_t capacity() const { return m_capacity_pow2 * kEntriesPerBucket; }

private:
    uint16_t* m_buckets = nullptr; 
    size_t m_capacity_pow2 = 0;
    size_t m_bucket_mask = 0;
    size_t m_count = 0;
    CuckooKernel m_kernel;

    // Inicializa os kernels baseado nas features da CPU
    void select_kernels(const system::HardwareInfo& hw);

    // Hashing helpers baseados em Zero-Cost (aproveitando o HASH160)
    static inline uint16_t derive_fingerprint(const uint8_t* data) {
        uint16_t fp = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        return (fp == 0) ? 1 : fp;
    }

    inline size_t derive_index(const uint8_t* data) const {
        uint64_t idx;
        std::memcpy(&idx, data + 2, 8);
        return idx & m_bucket_mask;
    }

    inline size_t alt_index(size_t i, uint16_t fp) const {
        // Multiplicador de mistura rápido para o índice alternativo
        return (i ^ (static_cast<size_t>(fp) * 0x5bd1e995)) & m_bucket_mask;
    }

    bool insert_internal(uint16_t fp, size_t i1);
};

} // namespace bchaves::core
