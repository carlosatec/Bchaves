/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Tabela Hash de endereçamento aberto (FlatMap) otimizada para armadilhas.
 *            Focada em cache locality e baixa fragmentação.
 */
#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include "core/secp256k1.hpp"
#include "system/hardware.hpp"

namespace bchaves::core {

struct TrapEntry {
    uint64_t x[4];
    uint64_t dist[4];
    uint8_t flags; // bit 0: occupied, bit 1: x_odd, bit 2: is_wild
};

class TrapTable {
public:
    TrapTable(size_t capacity) {
        // Garantir potência de 2 para mascaramento rápido
        m_capacity = 1;
        while (m_capacity < capacity) m_capacity <<= 1;
        m_mask = m_capacity - 1;
        
        m_alloc_size = (m_capacity * sizeof(TrapEntry)) + 64;
        m_entries = (TrapEntry*)bchaves::system::allocate_huge_pages(m_alloc_size);
        
        if (!m_entries) {
            // Fallback para alocação convencional
#if defined(_WIN32)
            m_entries = (TrapEntry*)_aligned_malloc(m_alloc_size, 64);
#else
            m_entries = (TrapEntry*)aligned_alloc(64, m_alloc_size);
#endif
            m_use_huge_pages = false;
        } else {
            m_use_huge_pages = true;
        }
        
        if (m_entries) {
            std::memset(m_entries, 0, m_alloc_size);
        }
    }

    ~TrapTable() {
        if (m_entries) {
            if (m_use_huge_pages) {
                bchaves::system::free_huge_pages(m_entries, m_alloc_size);
            } else {
#if defined(_WIN32)
                _aligned_free(m_entries);
#else
                free(m_entries);
#endif
            }
        }
    }

    bool insert(const BigInt& x, bool x_odd, const BigInt& dist, bool is_wild) {
        uint64_t h = x.limbs[0] & m_mask;
        for (size_t i = 0; i < 128; ++i) { // Linear probing limitado
            size_t idx = (h + i) & m_mask;
            if (!(m_entries[idx].flags & 1)) {
                std::memcpy(m_entries[idx].x, x.limbs.data(), 32);
                std::memcpy(m_entries[idx].dist, dist.limbs.data(), 32);
                m_entries[idx].flags = 1 | (x_odd ? 2 : 0) | (is_wild ? 4 : 0);
                return true;
            }
            // Se já existe (mesma chave), retornar true (sucesso silencioso)
            bool match = true;
            for(int j=0; j<4; ++j) if(m_entries[idx].x[j] != x.limbs[j]) { match = false; break; }
            if (match && ((m_entries[idx].flags & 2) != 0) == x_odd) return true;
        }
        return false; // Tabela cheia ou colisão persistente
    }

    bool lookup(const BigInt& x, bool x_odd, BigInt& out_dist, bool& out_is_wild) const {
        uint64_t h = x.limbs[0] & m_mask;
        for (size_t i = 0; i < 128; ++i) {
            size_t idx = (h + i) & m_mask;
            if (!(m_entries[idx].flags & 1)) return false;
            
            bool match = true;
            for(int j=0; j<4; ++j) if(m_entries[idx].x[j] != x.limbs[j]) { match = false; break; }
            if (match && ((m_entries[idx].flags & 2) != 0) == x_odd) {
                std::memcpy(out_dist.limbs.data(), m_entries[idx].dist, 32);
                out_is_wild = (m_entries[idx].flags & 4) != 0;
                return true;
            }
        }
        return false;
    }

    void clear() {
        std::memset(m_entries, 0, m_capacity * sizeof(TrapEntry));
    }

    size_t capacity() const { return m_capacity; }
    const TrapEntry* entries() const { return m_entries; }

private:
    TrapEntry* m_entries;
    size_t m_capacity;
    size_t m_mask;
    size_t m_alloc_size = 0;
    bool m_use_huge_pages = false;
};

} // namespace bchaves::core
