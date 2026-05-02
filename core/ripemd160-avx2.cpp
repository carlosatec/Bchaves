/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: RIPEMD160 implementation optimized for AVX2.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__AVX2__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace bchaves {
namespace core {
namespace ripemd160_avx2 {

static constexpr std::size_t kBlockSize = 64u;
static constexpr std::size_t kHashSize = 20u;

void hash8_ripemd160_avx2(const std::uint8_t* const data[8], std::uint8_t* const out[8]) {
    if (!data || !out) return;
    
    const __m256i bswap_mask = _mm256_set_epi8(
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
    );
    
    // Message schedule buffers: X[round][lane]
    alignas(32) std::uint32_t X[16][8];
    
    // Load 32 bytes (8 words) from each message, add padding
    for (int lane = 0; lane < 8; ++lane) {
        const std::uint32_t* p = reinterpret_cast<const std::uint32_t*>(data[lane]);
        for (int j = 0; j < 8; ++j) {
            X[j][lane] = p[j];
        }
        // Add padding: 0x80 at byte 32, then zeros, then 256-bit length at end
        X[8][lane] = 0x80000000u;
        for (int j = 9; j < 14; ++j) X[j][lane] = 0;
        X[14][lane] = 256u;  // message length in bits
        X[15][lane] = 0;
    }
    
    // Initial hash values
    __m256i h0 = _mm256_set1_epi32(0x67452301u);
    __m256i h1 = _mm256_set1_epi32(0xefcdab89u);
    __m256i h2 = _mm256_set1_epi32(0x98badcfeu);
    __m256i h3 = _mm256_set1_epi32(0x10325476u);
    __m256i h4 = _mm256_set1_epi32(0xc3d2e1f0u);
    
    __m256i al = h0, bl = h1, cl = h2, dl = h3, el = h4;
    __m256i ar = h0, br = h1, cr = h2, dr = h3, er = h4;
    
    // Message schedule permutations
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
    
    #define ADD _mm256_add_epi32
    #define XOR _mm256_xor_si256
    #define AND _mm256_and_si256
    #define OR _mm256_or_si256
    #define ANDNOT _mm256_andnot_si256
    #define ROTL_VAR(x, n) OR(_mm256_slli_epi32(x, n), _mm256_srli_epi32(x, 32 - n))
    #define ROTL10(x) OR(_mm256_slli_epi32(x, 10), _mm256_srli_epi32(x, 22))
    
    // Define the 5 Boolean functions
    #define F0(x, y, z) XOR(XOR(x, y), z)
    #define F1(x, y, z) OR(AND(x, y), ANDNOT(x, z))
    #define F2(x, y, z) XOR(OR(x, XOR(y, _mm256_set1_epi32(0xFFFFFFFFu))), z)
    #define F3(x, y, z) OR(AND(x, z), AND(y, XOR(z, _mm256_set1_epi32(0xFFFFFFFFu))))
    #define F4(x, y, z) XOR(x, OR(y, XOR(z, _mm256_set1_epi32(0xFFFFFFFFu))))
    
    // 80 rounds
    for (std::size_t j = 0; j < 80; ++j) {
        __m256i fj, fjp, kj, kjp;
        
        // Left line function selection
        if (j <= 15) {
            fj = F0(bl, cl, dl);
            kj = _mm256_setzero_si256();
        } else if (j <= 31) {
            fj = F1(bl, cl, dl);
            kj = _mm256_set1_epi32(0x5a827999u);
        } else if (j <= 47) {
            fj = F2(bl, cl, dl);
            kj = _mm256_set1_epi32(0x6ed9eba1u);
        } else if (j <= 63) {
            fj = F3(bl, cl, dl);
            kj = _mm256_set1_epi32(0x8f1bbcdcu);
        } else {
            fj = F4(bl, cl, dl);
            kj = _mm256_set1_epi32(0xa953fd4eu);
        }
        
        // Right line function selection (mirrored)
        if (j <= 15) {
            fjp = F4(br, cr, dr);
            kjp = _mm256_set1_epi32(0x50a28be6u);
        } else if (j <= 31) {
            fjp = F3(br, cr, dr);
            kjp = _mm256_set1_epi32(0x5c4dd124u);
        } else if (j <= 47) {
            fjp = F2(br, cr, dr);
            kjp = _mm256_set1_epi32(0x6d703ef3u);
        } else if (j <= 63) {
            fjp = F1(br, cr, dr);
            kjp = _mm256_set1_epi32(0x7a6d76e9u);
        } else {
            fjp = F0(br, cr, dr);
            kjp = _mm256_setzero_si256();
        }
        
        // Load message words for left and right schedules (one lane at a time)
        __m256i xj = _mm256_set_epi32(
            X[r[j]][7], X[r[j]][6], X[r[j]][5], X[r[j]][4],
            X[r[j]][3], X[r[j]][2], X[r[j]][1], X[r[j]][0]
        );
        __m256i xjp = _mm256_set_epi32(
            X[rp[j]][7], X[rp[j]][6], X[rp[j]][5], X[rp[j]][4],
            X[rp[j]][3], X[rp[j]][2], X[rp[j]][1], X[rp[j]][0]
        );
        
        // Left line round
        __m256i tl = ADD(al, ADD(fj, ADD(xj, kj)));
        tl = ADD(ROTL_VAR(tl, s[j]), el);
        al = el; el = dl; dl = ROTL10(cl); cl = bl; bl = tl;
        
        // Right line round
        __m256i tr = ADD(ar, ADD(fjp, ADD(xjp, kjp)));
        tr = ADD(ROTL_VAR(tr, sp[j]), er);
        ar = er; er = dr; dr = ROTL10(cr); cr = br; br = tr;
    }
    
    // Combine left and right results
    __m256i t = ADD(h1, ADD(cl, dr));
    h1 = ADD(h2, ADD(dl, er));
    h2 = ADD(h3, ADD(el, ar));
    h3 = ADD(h4, ADD(al, br));
    h4 = ADD(h0, ADD(bl, cr));
    h0 = t;
    
    // Store results
    std::uint32_t a0[8], a1[8], a2[8], a3[8], a4[8];
    _mm256_storeu_si256((__m256i*)a0, h0);
    _mm256_storeu_si256((__m256i*)a1, h1);
    _mm256_storeu_si256((__m256i*)a2, h2);
    _mm256_storeu_si256((__m256i*)a3, h3);
    _mm256_storeu_si256((__m256i*)a4, h4);
    
    for (int lane = 0; lane < 8; ++lane) {
        std::uint32_t* o = reinterpret_cast<std::uint32_t*>(out[lane]);
        o[0] = a0[lane]; o[1] = a1[lane]; o[2] = a2[lane];
        o[3] = a3[lane]; o[4] = a4[lane];
    }
}

}  // namespace ripemd160_avx2
}  // namespace core
}  // namespace bchaves

#endif  // defined(__AVX2__)