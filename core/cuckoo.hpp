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

// Simple Cuckoo Filter for fast discrete log lookups
class CuckooFilter {
public:
    CuckooFilter(size_t num_items) {
        // Each bucket has 4 slots, 16 bits per fingerprint
        capacity = (num_items * 10) / 9 / 4; // 90% load factor
        if (capacity < 1) capacity = 1;
        buckets.resize(capacity);
    }

    bool insert(uint64_t hash) {
        uint16_t fingerprint = (uint16_t)(hash & 0xFFFF);
        if (fingerprint == 0) fingerprint = 1;
        size_t h1 = (hash >> 16) % capacity;
        size_t h2 = (h1 ^ hash_fingerprint(fingerprint)) % capacity;

        if (add_to_bucket(h1, fingerprint)) return true;
        if (add_to_bucket(h2, fingerprint)) return true;

        // Kick out existing item (Cuckooing)
        size_t curr_h = h1;
        for (int i = 0; i < 500; ++i) {
            int slot = rand() % 4;
            std::swap(fingerprint, buckets[curr_h][slot]);
            curr_h = (curr_h ^ hash_fingerprint(fingerprint)) % capacity;
            if (add_to_bucket(curr_h, fingerprint)) return true;
        }
        return false;
    }

    bool lookup(uint64_t hash) const {
        uint16_t fingerprint = (uint16_t)(hash & 0xFFFF);
        if (fingerprint == 0) fingerprint = 1;
        size_t h1 = (hash >> 16) % capacity;
        size_t h2 = (h1 ^ hash_fingerprint(fingerprint)) % capacity;

        for (int i = 0; i < 4; ++i) {
            if (buckets[h1][i] == fingerprint || buckets[h2][i] == fingerprint) return true;
        }
        return false;
    }

private:
    struct Bucket {
        uint16_t slots[4]{0, 0, 0, 0};
        uint16_t& operator[](size_t i) { return slots[i]; }
        const uint16_t& operator[](size_t i) const { return slots[i]; }
    };

    size_t capacity;
    std::vector<Bucket> buckets;

    bool add_to_bucket(size_t h, uint16_t f) {
        for (int i = 0; i < 4; ++i) {
            if (buckets[h][i] == 0) {
                buckets[h][i] = f;
                return true;
            }
        }
        return false;
    }

    size_t hash_fingerprint(uint16_t f) const {
        return (size_t)f * 0x5bd1e995;
    }
};

} // namespace bchaves::core
