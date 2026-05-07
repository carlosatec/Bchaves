/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: FlatHashMap — open-addressing hash map com linear probing.
 *            Substitui std::unordered_map em hot paths para eliminar
 *            alocações de heap por nó e melhorar cache locality.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

namespace bchaves::core {

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class FlatHashMap {
public:
    struct Bucket {
        Key key;
        Value value;
        bool occupied = false;
    };

    explicit FlatHashMap(size_t initial_capacity = 1024) {
        // Round up to power of 2
        m_capacity = 1;
        while (m_capacity < initial_capacity) m_capacity <<= 1;
        m_mask = m_capacity - 1;
        m_buckets.resize(m_capacity);
        m_count = 0;
    }

    // Inserir ou atualizar uma entrada
    void insert(const Key& key, const Value& value) {
        if (m_count >= m_capacity * 3 / 4) {
            rehash(m_capacity * 2);
        }
        size_t idx = m_hasher(key) & m_mask;
        while (true) {
            auto& b = m_buckets[idx];
            if (!b.occupied) {
                b.key = key;
                b.value = value;
                b.occupied = true;
                ++m_count;
                return;
            }
            if (b.key == key) {
                b.value = value; // Update
                return;
            }
            idx = (idx + 1) & m_mask;
        }
    }

    // operator[] para compatibilidade com std::unordered_map
    Value& operator[](const Key& key) {
        if (m_count >= m_capacity * 3 / 4) {
            rehash(m_capacity * 2);
        }
        size_t idx = m_hasher(key) & m_mask;
        while (true) {
            auto& b = m_buckets[idx];
            if (!b.occupied) {
                b.key = key;
                b.value = Value{};
                b.occupied = true;
                ++m_count;
                return b.value;
            }
            if (b.key == key) {
                return b.value;
            }
            idx = (idx + 1) & m_mask;
        }
    }

    // Buscar uma entrada
    Value* find(const Key& key) {
        size_t idx = m_hasher(key) & m_mask;
        while (true) {
            auto& b = m_buckets[idx];
            if (!b.occupied) return nullptr;
            if (b.key == key) return &b.value;
            idx = (idx + 1) & m_mask;
        }
    }

    const Value* find(const Key& key) const {
        size_t idx = m_hasher(key) & m_mask;
        while (true) {
            auto& b = m_buckets[idx];
            if (!b.occupied) return nullptr;
            if (b.key == key) return &b.value;
            idx = (idx + 1) & m_mask;
        }
    }

    bool contains(const Key& key) const {
        return find(key) != nullptr;
    }

    size_t size() const { return m_count; }
    size_t capacity() const { return m_capacity; }

    void clear() {
        for (auto& b : m_buckets) b.occupied = false;
        m_count = 0;
    }

    // Iteração — suporta range-based for
    class iterator {
    public:
        using bucket_iter = typename std::vector<Bucket>::iterator;
        iterator(bucket_iter cur, bucket_iter end) : m_cur(cur), m_end(end) {
            advance_to_occupied();
        }
        std::pair<const Key&, Value&> operator*() { return {m_cur->key, m_cur->value}; }
        iterator& operator++() { ++m_cur; advance_to_occupied(); return *this; }
        bool operator!=(const iterator& other) const { return m_cur != other.m_cur; }
    private:
        void advance_to_occupied() {
            while (m_cur != m_end && !m_cur->occupied) ++m_cur;
        }
        bucket_iter m_cur, m_end;
    };

    iterator begin() { return iterator(m_buckets.begin(), m_buckets.end()); }
    iterator end()   { return iterator(m_buckets.end(), m_buckets.end()); }

private:
    void rehash(size_t new_capacity) {
        std::vector<Bucket> old_buckets = std::move(m_buckets);
        m_capacity = new_capacity;
        m_mask = m_capacity - 1;
        m_buckets.resize(m_capacity);
        m_count = 0;
        for (auto& b : old_buckets) {
            if (b.occupied) {
                insert(b.key, b.value);
            }
        }
    }

    std::vector<Bucket> m_buckets;
    Hash m_hasher;
    size_t m_capacity = 0;
    size_t m_mask = 0;
    size_t m_count = 0;
};

} // namespace bchaves::core
