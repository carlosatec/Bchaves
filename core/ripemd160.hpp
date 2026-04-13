#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bchaves::core {

inline std::uint32_t ripemd_rotl(std::uint32_t value, std::uint32_t shift) {
    return (value << shift) | (value >> (32u - shift));
}

inline std::uint32_t ripemd_f(std::size_t round, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    if (round <= 15u) return x ^ y ^ z;
    if (round <= 31u) return (x & y) | (~x & z);
    if (round <= 47u) return (x | ~y) ^ z;
    if (round <= 63u) return (x & z) | (y & ~z);
    return x ^ (y | ~z);
}

inline std::uint32_t ripemd_k(std::size_t round) {
    if (round <= 15u) return 0x00000000u;
    if (round <= 31u) return 0x5a827999u;
    if (round <= 47u) return 0x6ed9eba1u;
    if (round <= 63u) return 0x8f1bbcdcu;
    return 0xa953fd4eu;
}

inline std::uint32_t ripemd_kp(std::size_t round) {
    if (round <= 15u) return 0x50a28be6u;
    if (round <= 31u) return 0x5c4dd124u;
    if (round <= 47u) return 0x6d703ef3u;
    if (round <= 63u) return 0x7a6d76e9u;
    return 0x00000000u;
}

inline std::array<std::uint8_t, 20> ripemd160(const std::uint8_t* input, std::size_t length) {
    static constexpr std::uint32_t r[80] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8,
        3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12,
        1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2,
        4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13
    };
    static constexpr std::uint32_t rp[80] = {
        5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12,
        6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2,
        15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13,
        8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14,
        12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11
    };
    static constexpr std::uint32_t s[80] = {
        11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8,
        7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12,
        11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5,
        11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12,
        9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6
    };
    static constexpr std::uint32_t sp[80] = {
        8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6,
        9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11,
        9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5,
        15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8,
        8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11
    };

    std::uint32_t h0 = 0x67452301u, h1 = 0xefcdab89u, h2 = 0x98badcfeu, h3 = 0x10325476u, h4 = 0xc3d2e1f0u;

    // Fast padding for up to 55 bytes (supports SHA256 input case)
    std::uint8_t buffer[64]{};
    for (std::size_t i = 0; i < length && i < 64; ++i) buffer[i] = input[i];
    if (length < 64) {
        buffer[length] = 0x80u;
        if (length < 56) {
            const std::uint64_t bit_len = static_cast<std::uint64_t>(length) * 8u;
            for (int i = 0; i < 8; ++i) buffer[56 + i] = (bit_len >> (i * 8u)) & 0xffu;
        }
    }

    std::uint32_t x[16]{};
    for (std::size_t i = 0; i < 16; ++i) {
        x[i] = static_cast<std::uint32_t>(buffer[i * 4]) | (static_cast<std::uint32_t>(buffer[i * 4 + 1]) << 8u) |
               (static_cast<std::uint32_t>(buffer[i * 4 + 2]) << 16u) | (static_cast<std::uint32_t>(buffer[i * 4 + 3]) << 24u);
    }

    std::uint32_t al = h0, bl = h1, cl = h2, dl = h3, el = h4;
    std::uint32_t ar = h0, br = h1, cr = h2, dr = h3, er = h4;

    for (std::size_t j = 0; j < 80; ++j) {
        std::uint32_t tl = ripemd_rotl(al + ripemd_f(j, bl, cl, dl) + x[r[j] + ripemd_k(j), s[j]) + el;
        al = el; el = dl; dl = ripemd_rotl(cl, 10u); cl = bl; bl = tl;
        std::uint32_t tr = ripemd_rotl(ar + ripemd_f(79u - j, br, cr, dr) + x[rp[j]] + ripemd_kp(j), sp[j]) + er;
        ar = er; er = dr; dr = ripemd_rotl(cr, 10u); cr = br; br = tr;
    }

    const std::uint32_t t = h1 + cl + dr; h1 = h2 + dl + er; h2 = h3 + el + ar; h3 = h4 + al + br; h4 = h0 + bl + cr; h0 = t;

    std::array<std::uint8_t, 20> out{};
    const std::uint32_t state[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        out[i * 4] = state[i] & 0xffu; out[i * 4 + 1] = (state[i] >> 8) & 0xffu;
        out[i * 4 + 2] = (state[i] >> 16) & 0xffu; out[i * 4 + 3] = (state[i] >> 24) & 0xffu;
    }
    return out;
}

inline std::array<std::uint8_t, 20> ripemd160(const std::vector<std::uint8_t>& input) {
    return ripemd160(input.data(), input.size());
}

}  // namespace bchaves::core
