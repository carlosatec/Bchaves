/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: SHA-256 implementation optimized for AVX2 (256-bit.
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

namespace bchaves {
namespace core {
namespace sha256_avx2 {

static constexpr std::size_t kBlockSize = 64u;
static constexpr std::size_t kHashSize = 32u;

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

void hash8_avx2(const std::uint8_t* const data[8], std::size_t length, std::uint8_t* const out[8]) {
    if (length != 33 && length != 65) return;
    
    const __m256i bswap_mask = _mm256_set_epi8(
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
    );
    
    __m256i W[64];
    const __m256i zero = _mm256_setzero_si256();
    
    #define LOAD_W8(i) _mm256_set_epi32( \
        *(const std::uint32_t*)(data[7] + i*4), *(const std::uint32_t*)(data[6] + i*4), \
        *(const std::uint32_t*)(data[5] + i*4), *(const std::uint32_t*)(data[4] + i*4), \
        *(const std::uint32_t*)(data[3] + i*4), *(const std::uint32_t*)(data[2] + i*4), \
        *(const std::uint32_t*)(data[1] + i*4), *(const std::uint32_t*)(data[0] + i*4))
    
    #define ROTR8(x, n) _mm256_or_si256(_mm256_srli_epi32(x, n), _mm256_slli_epi32(x, 32 - n))
    #define SHR8(x, n) _mm256_srli_epi32(x, n)
    #define SIG0(x) _mm256_xor_si256(ROTR8(x, 7), _mm256_xor_si256(ROTR8(x, 18), SHR8(x, 3)))
    #define SIG1(x) _mm256_xor_si256(ROTR8(x, 17), _mm256_xor_si256(ROTR8(x, 19), SHR8(x, 10)))
    #define EP0(x) _mm256_xor_si256(ROTR8(x, 2), _mm256_xor_si256(ROTR8(x, 13), ROTR8(x, 22)))
    #define EP1(x) _mm256_xor_si256(ROTR8(x, 6), _mm256_xor_si256(ROTR8(x, 11), ROTR8(x, 25)))
    #define CH(e, f, g) _mm256_xor_si256(_mm256_and_si256(e, f), _mm256_andnot_si256(e, g))
    #define MAJ(a, b, c) _mm256_or_si256(_mm256_and_si256(a, b), _mm256_or_si256(_mm256_and_si256(a, c), _mm256_and_si256(b, c)))
    
    auto expand_schedule = [&]() {
        for (int i = 16; i < 64; ++i) {
            W[i] = _mm256_add_epi32(
                _mm256_add_epi32(SIG1(W[i - 2]), W[i - 7]),
                _mm256_add_epi32(SIG0(W[i - 15]), W[i - 16])
            );
        }
    };
    
    auto run_block = [&](__m256i state[8]) {
        __m256i A = state[0], B = state[1], C = state[2], D = state[3];
        __m256i E = state[4], F = state[5], G = state[6], H = state[7];
        const __m256i initA = A, initB = B, initC = C, initD = D;
        const __m256i initE = E, initF = F, initG = G, initH = H;
        
        for (int i = 0; i < 64; ++i) {
            __m256i T1 = _mm256_add_epi32(
                _mm256_add_epi32(_mm256_add_epi32(H, EP1(E)), CH(E, F, G)),
                _mm256_add_epi32(_mm256_set1_epi32(kTable[i]), W[i])
            );
            __m256i T2 = _mm256_add_epi32(EP0(A), MAJ(A, B, C));
            H = G; G = F; F = E; E = _mm256_add_epi32(D, T1);
            D = C; C = B; B = A; A = _mm256_add_epi32(T1, T2);
        }
        state[0] = _mm256_add_epi32(A, initA); state[1] = _mm256_add_epi32(B, initB);
        state[2] = _mm256_add_epi32(C, initC); state[3] = _mm256_add_epi32(D, initD);
        state[4] = _mm256_add_epi32(E, initE); state[5] = _mm256_add_epi32(F, initF);
        state[6] = _mm256_add_epi32(G, initG); state[7] = _mm256_add_epi32(H, initH);
    };
    
    __m256i state[8] = {
        _mm256_set1_epi32(kInitialHash[0]), _mm256_set1_epi32(kInitialHash[1]),
        _mm256_set1_epi32(kInitialHash[2]), _mm256_set1_epi32(kInitialHash[3]),
        _mm256_set1_epi32(kInitialHash[4]), _mm256_set1_epi32(kInitialHash[5]),
        _mm256_set1_epi32(kInitialHash[6]), _mm256_set1_epi32(kInitialHash[7])
    };
    
    static constexpr std::uint32_t kSingleBlockBits = 264u;
    static constexpr std::uint32_t kDoubleBlockBits = 520u;
    
    if (length == 33) {
        for (int i = 0; i < 8; ++i) W[i] = _mm256_shuffle_epi8(LOAD_W8(i), bswap_mask);
        W[8] = _mm256_set_epi32(
            ((std::uint32_t)data[7][32] << 24) | 0x00800000u, ((std::uint32_t)data[6][32] << 24) | 0x00800000u,
            ((std::uint32_t)data[5][32] << 24) | 0x00800000u, ((std::uint32_t)data[4][32] << 24) | 0x00800000u,
            ((std::uint32_t)data[3][32] << 24) | 0x00800000u, ((std::uint32_t)data[2][32] << 24) | 0x00800000u,
            ((std::uint32_t)data[1][32] << 24) | 0x00800000u, ((std::uint32_t)data[0][32] << 24) | 0x00800000u
        );
        for (int i = 9; i < 15; ++i) W[i] = zero;
        W[15] = _mm256_set1_epi32(kSingleBlockBits);
        expand_schedule(); run_block(state);
    } else {
        for (int i = 0; i < 16; ++i) W[i] = _mm256_shuffle_epi8(LOAD_W8(i), bswap_mask);
        expand_schedule(); run_block(state);
        W[0] = _mm256_set_epi32(
            ((std::uint32_t)data[7][64] << 24) | 0x00800000u, ((std::uint32_t)data[6][64] << 24) | 0x00800000u,
            ((std::uint32_t)data[5][64] << 24) | 0x00800000u, ((std::uint32_t)data[4][64] << 24) | 0x00800000u,
            ((std::uint32_t)data[3][64] << 24) | 0x00800000u, ((std::uint32_t)data[2][64] << 24) | 0x00800000u,
            ((std::uint32_t)data[1][64] << 24) | 0x00800000u, ((std::uint32_t)data[0][64] << 24) | 0x00800000u
        );
        for (int i = 1; i < 15; ++i) W[i] = zero;
        W[15] = _mm256_set1_epi32(kDoubleBlockBits);
        expand_schedule(); run_block(state);
    }
    
    #define STORE_H8(i, reg) do { \
        __m256i swapped = _mm256_shuffle_epi8(reg, bswap_mask); \
        std::uint32_t arr[8]; \
        _mm256_storeu_si256((__m256i*)arr, swapped); \
        ((std::uint32_t*)out[0])[i] = arr[0]; \
        ((std::uint32_t*)out[1])[i] = arr[1]; \
        ((std::uint32_t*)out[2])[i] = arr[2]; \
        ((std::uint32_t*)out[3])[i] = arr[3]; \
        ((std::uint32_t*)out[4])[i] = arr[4]; \
        ((std::uint32_t*)out[5])[i] = arr[5]; \
        ((std::uint32_t*)out[6])[i] = arr[6]; \
        ((std::uint32_t*)out[7])[i] = arr[7]; \
    } while(0)
    
    STORE_H8(0, state[0]); STORE_H8(1, state[1]); STORE_H8(2, state[2]); STORE_H8(3, state[3]);
    STORE_H8(4, state[4]); STORE_H8(5, state[5]); STORE_H8(6, state[6]); STORE_H8(7, state[7]);
}

}  // namespace sha256_avx2
}  // namespace core
}  // namespace bchaves

#endif  // defined(__AVX2__)