/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: SHA-256 implementation optimized for AVX-512 (512-bit).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#if defined(__AVX512F__)

#include <immintrin.h>
#include <cstdint>
#include <cstddef>

namespace bchaves {
namespace core {
namespace sha256_avx512 {

static constexpr std::size_t kBlockSize = 64u;
static constexpr std::size_t kHashSize = 32u;
static constexpr std::size_t kLanes = 16u;

// SHA-256 constants
static constexpr std::uint32_t kInitialHash[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

static constexpr std::uint32_t kTable[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4eu, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90beffEAU, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

void hash16_avx512(const std::uint8_t* const data[kLanes], std::size_t length, std::uint8_t* const out[kLanes]) {
    if (length != 33 && length != 65) return;
    
    const __m512i bswap_mask = _mm512_set_epi8(
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
    );
    
    __m512i W[64];
    const __m512i zero = _mm512_setzero_si512();
    
    #define ROTR16(x, n) _mm512_or_si512(_mm512_srli_epi32(x, n), _mm512_slli_epi32(x, 32 - n))
    #define SHR16(x, n) _mm512_srli_epi32(x, n)
    #define SIG0(x) _mm512_xor_si512(ROTR16(x, 7), _mm512_xor_si512(ROTR16(x, 18), SHR16(x, 3)))
    #define SIG1(x) _mm512_xor_si512(ROTR16(x, 17), _mm512_xor_si512(ROTR16(x, 19), SHR16(x, 10)))
    #define EP0(x) _mm512_xor_si512(ROTR16(x, 2), _mm512_xor_si512(ROTR16(x, 13), ROTR16(x, 22)))
    #define EP1(x) _mm512_xor_si512(ROTR16(x, 6), _mm512_xor_si512(ROTR16(x, 11), ROTR16(x, 25)))
    #define CH(e, f, g) _mm512_xor_si512(_mm512_and_si512(e, f), _mm512_andnot_si512(e, g))
    #define MAJ(a, b, c) _mm512_or_si512(_mm512_and_si512(a, b), _mm512_or_si512(_mm512_and_si512(a, c), _mm512_and_si512(b, c)))
    
    auto expand_schedule = [&]() {
        for (int i = 16; i < 64; ++i) {
            W[i] = _mm512_add_epi32(
                _mm512_add_epi32(SIG1(W[i - 2]), W[i - 7]),
                _mm512_add_epi32(SIG0(W[i - 15]), W[i - 16])
            );
        }
    };
    
    auto run_block = [&](__m512i state[8]) {
        __m512i A = state[0], B = state[1], C = state[2], D = state[3];
        __m512i E = state[4], F = state[5], G = state[6], H = state[7];
        const __m512i initA = A, initB = B, initC = C, initD = D;
        const __m512i initE = E, initF = F, initG = G, initH = H;
        
        for (int i = 0; i < 64; ++i) {
            __m512i T1 = _mm512_add_epi32(
                _mm512_add_epi32(_mm512_add_epi32(H, EP1(E)), CH(E, F, G)),
                _mm512_add_epi32(_mm512_set1_epi32(kTable[i]), W[i])
            );
            __m512i T2 = _mm512_add_epi32(EP0(A), MAJ(A, B, C));
            H = G; G = F; F = E; E = _mm512_add_epi32(D, T1);
            D = C; C = B; B = A; A = _mm512_add_epi32(T1, T2);
        }
        state[0] = _mm512_add_epi32(A, initA); state[1] = _mm512_add_epi32(B, initB);
        state[2] = _mm512_add_epi32(C, initC); state[3] = _mm512_add_epi32(D, initD);
        state[4] = _mm512_add_epi32(E, initE); state[5] = _mm512_add_epi32(F, initF);
        state[6] = _mm512_add_epi32(G, initG); state[7] = _mm512_add_epi32(H, initH);
    };
    
    __m512i state[8] = {
        _mm512_set1_epi32(kInitialHash[0]), _mm512_set1_epi32(kInitialHash[1]),
        _mm512_set1_epi32(kInitialHash[2]), _mm512_set1_epi32(kInitialHash[3]),
        _mm512_set1_epi32(kInitialHash[4]), _mm512_set1_epi32(kInitialHash[5]),
        _mm512_set1_epi32(kInitialHash[6]), _mm512_set1_epi32(kInitialHash[7])
    };
    
    for (int i = 0; i < 16; ++i) W[i] = zero;
    expand_schedule(); run_block(state);
    
    for (int w = 0; w < 8; ++w) {
        std::uint32_t arr[16];
        _mm512_storeu_si512(arr, state[w]);
        for (int lane = 0; lane < 16; ++lane) {
            ((std::uint32_t*)out[lane])[w] = arr[lane];
        }
    }
}

}  // namespace sha256_avx512
}  // namespace core
}  // namespace bchaves

#endif  // defined(__AVX512F__)