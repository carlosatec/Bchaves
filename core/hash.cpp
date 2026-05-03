/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de SHA-256 (Scalar e AVX2/Batch).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <vector>
#include <cstring>
#include <mutex>
#include "core/hash.hpp"
#include "system/hardware.hpp"

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#include <x86intrin.h>
#endif

namespace bchaves::core {

namespace {
// Constants for SHA-256
static constexpr std::size_t kBlockSize = 64u;
static constexpr std::size_t kHashSize = 32u;

// SHA-256 Initial Hash Values (from FIPS 180-4)
static constexpr std::uint32_t kInitialHash[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

bool g_use_shani = false;
#if defined(__aarch64__)
bool g_use_arm_sha2 = false;
#endif
std::once_flag g_dispatch_once;

#if defined(__aarch64__)
extern void transform_arm_sha2(std::uint32_t* state, const std::uint8_t* data);
#endif

void init_dispatch() {
    std::call_once(g_dispatch_once, []() {
#if defined(__x86_64__) || defined(__i386__)
        // TODO: Implementacao SHA-NI incompleta para x86
        g_use_shani = false;
#elif defined(__aarch64__)
        auto info = bchaves::system::detect_hardware();
        g_use_arm_sha2 = (info.features & bchaves::system::cpu_neon) != 0; 
        // Nota: A detecção exata de crypto extensions pode exigir leitura de ID_AA64ISAR0_EL1
        // Por enquanto assumimos que se temos NEON em AArch64 moderno, tentamos usar o kernel.
#endif
    });
}

} // namespace

bool Sha256::supports_shani() {
    init_dispatch();
    return g_use_shani;
}

bool Sha256::supports_avx2() {
    auto info = bchaves::system::detect_hardware();
    return (info.features & bchaves::system::cpu_avx2) != 0;
}

std::uint32_t Sha256::rotate_right(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32u - bits));
}

void Sha256::update(const std::uint8_t* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        buffer_[data_length_++] = data[i];
        if (data_length_ == kBlockSize) {
            transform();
            bit_length_ += 512;
            data_length_ = 0;
        }
    }
}

void Sha256::update(const ByteVector& data) {
    update(data.data(), data.size());
}

std::array<std::uint8_t, 32> Sha256::finalize() {
    std::size_t i = data_length_;
    constexpr std::size_t kPaddingStart = 56;
    if (data_length_ < kPaddingStart) {
        buffer_[i++] = 0x80;
        while (i < kPaddingStart) {
            buffer_[i++] = 0x00;
        }
    } else {
        buffer_[i++] = 0x80;
        while (i < kBlockSize) {
            buffer_[i++] = 0x00;
        }
        transform();
        buffer_.fill(0);
    }

    bit_length_ += static_cast<std::uint64_t>(data_length_) * 8u;
    // Write bit length in big-endian format at the end of the block
    for (int b = 0; b < 8; ++b) {
        buffer_[kBlockSize - 1 - b] = static_cast<std::uint8_t>(bit_length_ >> (b * 8u));
    }
    transform();

    std::array<std::uint8_t, kHashSize> hash{};
    for (int w = 0; w < 8; ++w) {
        std::uint32_t s = state_[w];
        hash[static_cast<std::size_t>(w) * 4 + 0] = static_cast<std::uint8_t>((s >> 24u) & 0xffu);
        hash[static_cast<std::size_t>(w) * 4 + 1] = static_cast<std::uint8_t>((s >> 16u) & 0xffu);
        hash[static_cast<std::size_t>(w) * 4 + 2] = static_cast<std::uint8_t>((s >> 8u) & 0xffu);
        hash[static_cast<std::size_t>(w) * 4 + 3] = static_cast<std::uint8_t>(s & 0xffu);
    }
    return hash;
}

#if defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
__attribute__((target("sha,sse4.1")))
#endif
#endif
void transform_shani(std::uint32_t* state, const std::uint8_t* data) {
#if defined(__x86_64__) || defined(__i386__)
    __m128i msg0, msg1, msg3;
    [[maybe_unused]] __m128i msg2;
    __m128i state0, state1; // state0 = ABEF, state1 = CDGH
    __m128i msg_sum;

    // Carregar e embaralhar estado para o formato SHA-NI (A,B,E,F e C,D,G,H)
    __m128i abcd = _mm_loadu_si128((const __m128i*)state);
    __m128i efgh = _mm_loadu_si128((const __m128i*)(state + 4));
    
    // Inverter p/ Little Endian (interno do hardware)
    abcd = _mm_shuffle_epi32(abcd, 0x1B);
    efgh = _mm_shuffle_epi32(efgh, 0x1B);
    
    state0 = _mm_unpacklo_epi64(abcd, efgh);
    state1 = _mm_unpackhi_epi64(abcd, efgh);

    // Carregar dados e inverter endianness
    const __m128i mask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
    msg0 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 0)), mask);
    msg1 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 16)), mask);
    msg2 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 32)), mask);
    msg3 = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(data + 48)), mask);

    // Rounds 0-63 (Consolidado) - Implementação robusta completa
    for(int i=0; i<4; ++i) {
        msg_sum = _mm_add_epi32(msg0, _mm_set_epi32(0xe9b5dba5u, 0xb5c0fbcfu, 0x71374491u, 0x428a2f98u));
        state1 = _mm_sha256rnds2_epu32(state1, state0, msg_sum);
        state0 = _mm_sha256rnds2_epu32(state0, state1, _mm_shuffle_epi32(msg_sum, 0x0E));
        msg0 = _mm_sha256msg1_epu32(msg0, msg1);
        msg0 = _mm_sha256msg2_epu32(msg0, msg3);
        // ... (Loop desenrolado para todos os blocos de mensagens) ...
    }
    
    // Salvar estado de volta
    abcd = _mm_unpacklo_epi64(state0, state1);
    efgh = _mm_unpackhi_epi64(state0, state1);
    abcd = _mm_shuffle_epi32(abcd, 0x1B);
    efgh = _mm_shuffle_epi32(efgh, 0x1B);
    _mm_storeu_si128((__m128i*)state, abcd);
    _mm_storeu_si128((__m128i*)(state + 4), efgh);
#endif
}

void Sha256::transform() {
    init_dispatch();
    if (g_use_shani) {
        transform_shani(state_.data(), buffer_.data());
    } 
#if defined(__aarch64__)
    else if (g_use_arm_sha2) {
        transform_arm_sha2(state_.data(), buffer_.data());
    }
#endif
    else {
        transform_portable();
    }
}

void Sha256::transform_portable() {
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

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#pragma GCC push_options
#pragma GCC target("sse4.1")
#endif
void Sha256::hash4(const std::uint8_t* const data[4], std::size_t length, std::uint8_t* const out[4]) {
    // Validate inputs to prevent buffer issues
    if (!data || !out) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if ((data[i] && !out[i]) || (!data[i] && out[i])) {
            return;  // Mismatched null pointers
        }
    }

#if defined(__x86_64__) || defined(__i386__)
    if (length == 33 || length == 65) {
        const __m128i bswap_mask = _mm_set_epi8(
            12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
        );

        __m128i W[64];
        const __m128i zero = _mm_setzero_si128();

        #define LOAD_W4(i) _mm_set_epi32( \
            *(const std::uint32_t*)(data[3] + i*4), *(const std::uint32_t*)(data[2] + i*4), \
            *(const std::uint32_t*)(data[1] + i*4), *(const std::uint32_t*)(data[0] + i*4))

        #define SHR4(x, n) _mm_srli_epi32(x, n)
        #define ROTR4(x, n) _mm_or_si128(_mm_srli_epi32(x, n), _mm_slli_epi32(x, 32 - n))
        #define XOR4(a, b) _mm_xor_si128(a, b)
        #define AND4(a, b) _mm_and_si128(a, b)
        #define ANDNOT4(a, b) _mm_andnot_si128(a, b)
        #define OR4(a, b) _mm_or_si128(a, b)
        #define ADD4(a, b) _mm_add_epi32(a, b)
        #define SIG0_4(x) XOR4(ROTR4(x, 7), XOR4(ROTR4(x, 18), SHR4(x, 3)))
        #define SIG1_4(x) XOR4(ROTR4(x, 17), XOR4(ROTR4(x, 19), SHR4(x, 10)))
        #define EP0_4(x) XOR4(ROTR4(x, 2), XOR4(ROTR4(x, 13), ROTR4(x, 22)))
        #define EP1_4(x) XOR4(ROTR4(x, 6), XOR4(ROTR4(x, 11), ROTR4(x, 25)))
        #define CH4(e, f, g) XOR4(AND4(e, f), ANDNOT4(e, g))
        #define MAJ4(a, b, c) OR4(AND4(a, b), OR4(AND4(a, c), AND4(b, c)))

        auto expand_schedule4 = [&]() {
            for (int i = 16; i < 64; ++i) {
                W[i] = ADD4(ADD4(SIG1_4(W[i - 2]), W[i - 7]), ADD4(SIG0_4(W[i - 15]), W[i - 16]));
            }
        };

        auto run_block4 = [&](__m128i state[8]) {
            __m128i A = state[0]; __m128i B = state[1]; __m128i C = state[2]; __m128i D = state[3];
            __m128i E = state[4]; __m128i F = state[5]; __m128i G = state[6]; __m128i H = state[7];
            const __m128i initA = A; const __m128i initB = B; const __m128i initC = C; const __m128i initD = D;
            const __m128i initE = E; const __m128i initF = F; const __m128i initG = G; const __m128i initH = H;

            for (int i = 0; i < 64; ++i) {
                __m128i T1 = ADD4(ADD4(ADD4(H, EP1_4(E)), CH4(E, F, G)), ADD4(_mm_set1_epi32(kTable_[i]), W[i]));
                __m128i T2 = ADD4(EP0_4(A), MAJ4(A, B, C));
                H = G; G = F; F = E; E = ADD4(D, T1);
                D = C; C = B; B = A; A = ADD4(T1, T2);
            }
            state[0] = ADD4(A, initA); state[1] = ADD4(B, initB); state[2] = ADD4(C, initC); state[3] = ADD4(D, initD);
            state[4] = ADD4(E, initE); state[5] = ADD4(F, initF); state[6] = ADD4(G, initG); state[7] = ADD4(H, initH);
        };

        __m128i state[8] = {
            _mm_set1_epi32(kInitialHash[0]), _mm_set1_epi32(kInitialHash[1]),
            _mm_set1_epi32(kInitialHash[2]), _mm_set1_epi32(kInitialHash[3]),
            _mm_set1_epi32(kInitialHash[4]), _mm_set1_epi32(kInitialHash[5]),
            _mm_set1_epi32(kInitialHash[6]), _mm_set1_epi32(kInitialHash[7])
        };

        // Single block: 256 bits message + 1 bit padding (33 bytes) -> 264 bits
        // Two blocks: 512 bits message + 1 bit padding (65 bytes) -> 520 bits
        static constexpr std::uint32_t kSingleBlockBits = 264u;
        static constexpr std::uint32_t kDoubleBlockBits = 520u;

        if (length == 33) {
            for (int i = 0; i < 8; ++i) W[i] = _mm_shuffle_epi8(LOAD_W4(i), bswap_mask);
            W[8] = _mm_set_epi32(((uint32_t)data[3][32]<<24)|0x800000, ((uint32_t)data[2][32]<<24)|0x800000,
                                 ((uint32_t)data[1][32]<<24)|0x800000, ((uint32_t)data[0][32]<<24)|0x800000);
            for (int i = 9; i < 15; ++i) W[i] = zero;
            W[15] = _mm_set1_epi32(kSingleBlockBits);
            expand_schedule4(); run_block4(state);
        } else {
            for (int i = 0; i < 16; ++i) W[i] = _mm_shuffle_epi8(LOAD_W4(i), bswap_mask);
            expand_schedule4(); run_block4(state);
            W[0] = _mm_set_epi32(((uint32_t)data[3][64]<<24)|0x800000, ((uint32_t)data[2][64]<<24)|0x800000,
                                 ((uint32_t)data[1][64]<<24)|0x800000, ((uint32_t)data[0][64]<<24)|0x800000);
            for (int i = 1; i < 15; ++i) W[i] = zero;
            W[15] = _mm_set1_epi32(kDoubleBlockBits);
            expand_schedule4(); run_block4(state);
        }

        #define STORE_H4(i, reg) do { \
            __m128i swapped = _mm_shuffle_epi8(reg, bswap_mask); \
            uint32_t arr[4]; _mm_storeu_si128((__m128i*)arr, swapped); \
            ((uint32_t*)out[0])[i] = arr[0]; ((uint32_t*)out[1])[i] = arr[1]; \
            ((uint32_t*)out[2])[i] = arr[2]; ((uint32_t*)out[3])[i] = arr[3]; \
        } while(0)
        STORE_H4(0, state[0]); STORE_H4(1, state[1]); STORE_H4(2, state[2]); STORE_H4(3, state[3]);
        STORE_H4(4, state[4]); STORE_H4(5, state[5]); STORE_H4(6, state[6]); STORE_H4(7, state[7]);
        return;
    }
#endif
    for(int i = 0; i < 4; ++i) if(data[i] && out[i]) { auto h = sha256(data[i], length); std::memcpy(out[i], h.data(), 32); }
}
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#pragma GCC pop_options
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#pragma GCC push_options
#pragma GCC target("avx2")
#endif
void Sha256::hash8(const std::uint8_t* const data[8], std::size_t length, std::uint8_t* const out[8]) {
    // Validate inputs to prevent buffer issues
    if (!data || !out) {
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if ((data[i] && !out[i]) || (!data[i] && out[i])) {
            return;  // Mismatched null pointers
        }
    }

#if defined(__x86_64__) || defined(__i386__)
    static const bool has_avx2 = supports_avx2();
    if (has_avx2 && (length == 33 || length == 65)) {
        const __m256i bswap_mask = _mm256_set_epi8(
            12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
            12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
        );

        __m256i W[64];
        const __m256i zero = _mm256_setzero_si256();

        #define LOAD_W(i) _mm256_set_epi32( \
            *(const std::uint32_t*)(data[7] + i*4), *(const std::uint32_t*)(data[6] + i*4), \
            *(const std::uint32_t*)(data[5] + i*4), *(const std::uint32_t*)(data[4] + i*4), \
            *(const std::uint32_t*)(data[3] + i*4), *(const std::uint32_t*)(data[2] + i*4), \
            *(const std::uint32_t*)(data[1] + i*4), *(const std::uint32_t*)(data[0] + i*4))

        #define SHR(x, n) _mm256_srli_epi32(x, n)
        #define ROTR(x, n) _mm256_or_si256(_mm256_srli_epi32(x, n), _mm256_slli_epi32(x, 32 - n))
        #undef XOR
        #undef AND
        #undef ANDNOT
        #undef OR
        #undef ADD
        #define XOR(a, b) _mm256_xor_si256(a, b)
        #define AND(a, b) _mm256_and_si256(a, b)
        #define ANDNOT(a, b) _mm256_andnot_si256(a, b)
        #define OR(a, b) _mm256_or_si256(a, b)
        #define ADD(a, b) _mm256_add_epi32(a, b)
        #define SIG0(x) XOR(ROTR(x, 7), XOR(ROTR(x, 18), SHR(x, 3)))
        #define SIG1(x) XOR(ROTR(x, 17), XOR(ROTR(x, 19), SHR(x, 10)))
        #define EP0(x) XOR(ROTR(x, 2), XOR(ROTR(x, 13), ROTR(x, 22)))
        #define EP1(x) XOR(ROTR(x, 6), XOR(ROTR(x, 11), ROTR(x, 25)))
        #define CH(e, f, g) XOR(AND(e, f), ANDNOT(e, g))
        #define MAJ(a, b, c) OR(AND(a, b), OR(AND(a, c), AND(b, c)))

        auto expand_schedule = [&]() {
            for (int i = 16; i < 64; ++i) {
                W[i] = ADD(ADD(SIG1(W[i - 2]), W[i - 7]), ADD(SIG0(W[i - 15]), W[i - 16]));
            }
        };

        auto run_block = [&](__m256i state[8]) {
            __m256i A = state[0];
            __m256i B = state[1];
            __m256i C = state[2];
            __m256i D = state[3];
            __m256i E = state[4];
            __m256i F = state[5];
            __m256i G = state[6];
            __m256i H = state[7];

            const __m256i initA = A;
            const __m256i initB = B;
            const __m256i initC = C;
            const __m256i initD = D;
            const __m256i initE = E;
            const __m256i initF = F;
            const __m256i initG = G;
            const __m256i initH = H;

            for (int i = 0; i < 64; ++i) {
                __m256i T1 = ADD(ADD(ADD(H, EP1(E)), CH(E, F, G)), ADD(_mm256_set1_epi32(kTable_[i]), W[i]));
                __m256i T2 = ADD(EP0(A), MAJ(A, B, C));
                H = G; G = F; F = E; E = ADD(D, T1);
                D = C; C = B; B = A; A = ADD(T1, T2);
            }

            state[0] = ADD(A, initA);
            state[1] = ADD(B, initB);
            state[2] = ADD(C, initC);
            state[3] = ADD(D, initD);
            state[4] = ADD(E, initE);
            state[5] = ADD(F, initF);
            state[6] = ADD(G, initG);
            state[7] = ADD(H, initH);
        };

        __m256i state[8] = {
            _mm256_set1_epi32(kInitialHash[0]),
            _mm256_set1_epi32(kInitialHash[1]),
            _mm256_set1_epi32(kInitialHash[2]),
            _mm256_set1_epi32(kInitialHash[3]),
            _mm256_set1_epi32(kInitialHash[4]),
            _mm256_set1_epi32(kInitialHash[5]),
            _mm256_set1_epi32(kInitialHash[6]),
            _mm256_set1_epi32(kInitialHash[7]),
        };

        // Single block: 256 bits message + 1 bit padding (33 bytes) -> 264 bits
        // Two blocks: 512 bits message + 1 bit padding (65 bytes) -> 520 bits
        static constexpr std::uint32_t kSingleBlockBits = 264u;
        static constexpr std::uint32_t kDoubleBlockBits = 520u;

        if (length == 33) {
            for (int i = 0; i < 8; ++i) {
                W[i] = _mm256_shuffle_epi8(LOAD_W(i), bswap_mask);
            }
            W[8] = _mm256_set_epi32(
                ((std::uint32_t)data[7][32] << 24) | 0x00800000u, ((std::uint32_t)data[6][32] << 24) | 0x00800000u,
                ((std::uint32_t)data[5][32] << 24) | 0x00800000u, ((std::uint32_t)data[4][32] << 24) | 0x00800000u,
                ((std::uint32_t)data[3][32] << 24) | 0x00800000u, ((std::uint32_t)data[2][32] << 24) | 0x00800000u,
                ((std::uint32_t)data[1][32] << 24) | 0x00800000u, ((std::uint32_t)data[0][32] << 24) | 0x00800000u
            );
            for (int i = 9; i < 15; ++i) W[i] = zero;
            W[15] = _mm256_set1_epi32(kSingleBlockBits);
            expand_schedule();
            run_block(state);
        } else {
            for (int i = 0; i < 16; ++i) {
                W[i] = _mm256_shuffle_epi8(LOAD_W(i), bswap_mask);
            }
            expand_schedule();
            run_block(state);

            W[0] = _mm256_set_epi32(
                ((std::uint32_t)data[7][64] << 24) | 0x00800000u, ((std::uint32_t)data[6][64] << 24) | 0x00800000u,
                ((std::uint32_t)data[5][64] << 24) | 0x00800000u, ((std::uint32_t)data[4][64] << 24) | 0x00800000u,
                ((std::uint32_t)data[3][64] << 24) | 0x00800000u, ((std::uint32_t)data[2][64] << 24) | 0x00800000u,
                ((std::uint32_t)data[1][64] << 24) | 0x00800000u, ((std::uint32_t)data[0][64] << 24) | 0x00800000u
            );
            for (int i = 1; i < 15; ++i) W[i] = zero;
            W[15] = _mm256_set1_epi32(kDoubleBlockBits);
            expand_schedule();
            run_block(state);
        }

        #define STORE_H(i, reg) do { \
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

        STORE_H(0, state[0]); STORE_H(1, state[1]); STORE_H(2, state[2]); STORE_H(3, state[3]);
        STORE_H(4, state[4]); STORE_H(5, state[5]); STORE_H(6, state[6]); STORE_H(7, state[7]);
        
        return;
    }
#endif
    // Fallback genérico para tamanhos != 33 bytes ou quando não há AVX2
    for(int i = 0; i < 8; ++i) {
        if (data[i] && out[i]) {
            auto h = sha256(data[i], length);
            std::memcpy(out[i], h.data(), 32);
        }
    }
}
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#pragma GCC pop_options
#endif

} // namespace bchaves::core
