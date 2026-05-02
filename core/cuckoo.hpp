/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de Cuckoo Filter para busca probabilística ultra-rápida.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>

namespace bchaves::core {

// Simple Cuckoo Filter for fast discrete log and address lookups
class CuckooFilter {
public:
    static constexpr size_t kEntriesPerBucket = 4;
    static constexpr size_t kMaxCuckooCount = 500;

    struct Bucket {
        uint16_t slots[kEntriesPerBucket]{0, 0, 0, 0};
        
        bool contains(uint16_t fp) const {
            for (size_t i = 0; i < kEntriesPerBucket; ++i) {
                if (slots[i] == fp) return true;
            }
            return false;
        }

        bool insert(uint16_t fp) {
            for (size_t i = 0; i < kEntriesPerBucket; ++i) {
                if (slots[i] == 0) {
                    slots[i] = fp;
                    return true;
                }
            }
            return false;
        }

        uint16_t swap(size_t idx, uint16_t fp) {
            uint16_t old = slots[idx];
            slots[idx] = fp;
            return old;
        }
    };

    CuckooFilter(size_t num_items) {
        // Aloca com 20% de margem para manter load factor saudável (~80-85%)
        size_t num_buckets = (num_items / kEntriesPerBucket) * 1.2;
        // Arredonda para potência de 2 para bitmask rápido
        m_capacity_pow2 = 1;
        while (m_capacity_pow2 < num_buckets) m_capacity_pow2 <<= 1;
        
        m_buckets.resize(m_capacity_pow2);
        m_bucket_mask = m_capacity_pow2 - 1;
        m_count = 0;
    }

    // Inserção via HASH160 (20 bytes)
    bool insert(const uint8_t* hash160) {
        uint16_t fp = derive_fingerprint(hash160);
        size_t i1 = derive_index(hash160);
        size_t i2 = alt_index(i1, fp);

        if (m_buckets[i1].insert(fp) || m_buckets[i2].insert(fp)) {
            m_count++;
            return true;
        }

        // Deslocamento Cuckoo
        size_t cur_i = (hash160[19] & 1) ? i1 : i2;
        for (size_t n = 0; n < kMaxCuckooCount; ++n) {
            size_t slot = (n + hash160[18]) % kEntriesPerBucket;
            fp = m_buckets[cur_i].swap(slot, fp);
            cur_i = alt_index(cur_i, fp);
            if (m_buckets[cur_i].insert(fp)) {
                m_count++;
                return true;
            }
        }
        return false;
    }

    // Lookup individual
    bool lookup(const uint8_t* hash160) const {
        uint16_t fp = derive_fingerprint(hash160);
        size_t i1 = derive_index(hash160);
        return lookup_internal(fp, i1);
    }

    // Lookup interno para uso com SIMD
    inline bool lookup_internal(uint16_t fp, size_t i1) const {
        size_t i2 = alt_index(i1, fp);
        return m_buckets[i1].contains(fp) || m_buckets[i2].contains(fp);
    }

    // Lookup em lote (8 itens)
    void lookup_batch8(const uint16_t fp[8], const uint64_t idx[8], bool results[8]) const {
        for (int i = 0; i < 8; ++i) {
            results[i] = lookup_internal(fp[i], idx[i]);
        }
    }

    // Sobrecarga para compatibilidade com uint64_t (Trap Tables)
    bool insert(uint64_t val) {
        uint8_t buf[20] = {0};
        std::memcpy(buf, &val, 8);
        return insert(buf);
    }

    bool lookup(uint64_t val) const {
        uint8_t buf[20] = {0};
        std::memcpy(buf, &val, 8);
        return lookup(buf);
    }

    size_t size() const { return m_count; }
    size_t capacity() const { return m_buckets.size() * kEntriesPerBucket; }

private:
    std::vector<Bucket> m_buckets;
    size_t m_bucket_mask;
    size_t m_capacity_pow2;
    size_t m_count;

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
        return (i ^ (static_cast<size_t>(fp) * 0x5bd1e995)) & m_bucket_mask;
    }
};

} // namespace bchaves::core
