#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ripemd160.hpp"

namespace bchaves::core {

using ByteVector = std::vector<std::uint8_t>;

class Sha256 {
public:
    static bool supports_shani();
    static bool supports_avx2();
    
    // Processamento em lote (8 hashes simultâneos via AVX2)
    static void hash8(const std::uint8_t* const data[8], std::size_t length, std::uint8_t* const out[8]);
    
    Sha256() { reset(); }

    void reset() {
        data_length_ = 0;
        bit_length_ = 0;
        state_[0] = 0x6a09e667u;
        state_[1] = 0xbb67ae85u;
        state_[2] = 0x3c6ef372u;
        state_[3] = 0xa54ff53au;
        state_[4] = 0x510e527fu;
        state_[5] = 0x9b05688cu;
        state_[6] = 0x1f83d9abu;
        state_[7] = 0x5be0cd19u;
        buffer_.fill(0);
    }

    void update(const std::uint8_t* data, std::size_t length);
    void update(const ByteVector& data);

    std::array<std::uint8_t, 32> finalize();

private:
    static constexpr std::array<std::uint32_t, 64> kTable_ = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    void transform();
    void transform_portable();
    static std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits);

    std::array<std::uint8_t, 64> buffer_{};
    std::array<std::uint32_t, 8> state_{};
    std::size_t data_length_{};
    std::uint64_t bit_length_{};
};

inline std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t length) {
    Sha256 ctx;
    ctx.update(data, length);
    return ctx.finalize();
}

inline std::array<std::uint8_t, 32> sha256(const ByteVector& data) {
    return sha256(data.data(), data.size());
}

inline std::array<std::uint8_t, 32> double_sha256(const ByteVector& data) {
    const auto first = sha256(data);
    return sha256(first.data(), first.size());
}

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t length) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1u) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

inline std::uint32_t crc32(const ByteVector& data) {
    return crc32(data.data(), data.size());
}

inline std::string to_lower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

inline bool is_hex(const std::string& value) {
    for (const char ch : value) {
        const bool digit = ch >= '0' && ch <= '9';
        const bool lower = ch >= 'a' && ch <= 'f';
        const bool upper = ch >= 'A' && ch <= 'F';
        if (!(digit || lower || upper)) {
            return false;
        }
    }
    return !value.empty();
}

inline ByteVector from_hex(const std::string& value) {
    ByteVector out;
    if ((value.size() % 2) != 0 || !is_hex(value)) {
        return out;
    }
    out.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        const auto nibble = [](char c) -> std::uint8_t {
            if (c >= '0' && c <= '9') {
                return static_cast<std::uint8_t>(c - '0');
            }
            if (c >= 'a' && c <= 'f') {
                return static_cast<std::uint8_t>(10 + c - 'a');
            }
            return static_cast<std::uint8_t>(10 + c - 'A');
        };
        out.push_back(static_cast<std::uint8_t>((nibble(value[i]) << 4u) | nibble(value[i + 1])));
    }
    return out;
}

}  // namespace bchaves::core
