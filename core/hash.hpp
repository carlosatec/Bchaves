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

    void update(const std::uint8_t* data, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            buffer_[data_length_++] = data[i];
            if (data_length_ == 64) {
                transform();
                bit_length_ += 512;
                data_length_ = 0;
            }
        }
    }

    void update(const ByteVector& data) {
        update(data.data(), data.size());
    }

    std::array<std::uint8_t, 32> finalize() {
        std::size_t i = data_length_;
        if (data_length_ < 56) {
            buffer_[i++] = 0x80;
            while (i < 56) {
                buffer_[i++] = 0x00;
            }
        } else {
            buffer_[i++] = 0x80;
            while (i < 64) {
                buffer_[i++] = 0x00;
            }
            transform();
            buffer_.fill(0);
        }

        bit_length_ += static_cast<std::uint64_t>(data_length_) * 8u;
        buffer_[63] = static_cast<std::uint8_t>(bit_length_);
        buffer_[62] = static_cast<std::uint8_t>(bit_length_ >> 8u);
        buffer_[61] = static_cast<std::uint8_t>(bit_length_ >> 16u);
        buffer_[60] = static_cast<std::uint8_t>(bit_length_ >> 24u);
        buffer_[59] = static_cast<std::uint8_t>(bit_length_ >> 32u);
        buffer_[58] = static_cast<std::uint8_t>(bit_length_ >> 40u);
        buffer_[57] = static_cast<std::uint8_t>(bit_length_ >> 48u);
        buffer_[56] = static_cast<std::uint8_t>(bit_length_ >> 56u);
        transform();

        std::array<std::uint8_t, 32> hash{};
        for (i = 0; i < 4; ++i) {
            hash[i] = static_cast<std::uint8_t>((state_[0] >> (24u - i * 8u)) & 0xffu);
            hash[i + 4] = static_cast<std::uint8_t>((state_[1] >> (24u - i * 8u)) & 0xffu);
            hash[i + 8] = static_cast<std::uint8_t>((state_[2] >> (24u - i * 8u)) & 0xffu);
            hash[i + 12] = static_cast<std::uint8_t>((state_[3] >> (24u - i * 8u)) & 0xffu);
            hash[i + 16] = static_cast<std::uint8_t>((state_[4] >> (24u - i * 8u)) & 0xffu);
            hash[i + 20] = static_cast<std::uint8_t>((state_[5] >> (24u - i * 8u)) & 0xffu);
            hash[i + 24] = static_cast<std::uint8_t>((state_[6] >> (24u - i * 8u)) & 0xffu);
            hash[i + 28] = static_cast<std::uint8_t>((state_[7] >> (24u - i * 8u)) & 0xffu);
        }
        return hash;
    }

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

    static std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits) {
        return (value >> bits) | (value << (32u - bits));
    }

    void transform() {
        std::uint32_t m[64]{};
        for (std::size_t i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<std::uint32_t>(buffer_[j]) << 24u) |
                   (static_cast<std::uint32_t>(buffer_[j + 1]) << 16u) |
                   (static_cast<std::uint32_t>(buffer_[j + 2]) << 8u) |
                   static_cast<std::uint32_t>(buffer_[j + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotate_right(m[i - 15], 7u) ^ rotate_right(m[i - 15], 18u) ^ (m[i - 15] >> 3u);
            const std::uint32_t s1 = rotate_right(m[i - 2], 17u) ^ rotate_right(m[i - 2], 19u) ^ (m[i - 2] >> 10u);
            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + ch + kTable_[i] + m[i];
            const std::uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

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
