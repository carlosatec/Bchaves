/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de SHA-256 utilizando extensões ARMv8 Crypto.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <cstdint>

#if defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO)
#include <arm_neon.h>

namespace bchaves::core {

static const uint32_t kSHA256Table[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, a2bfe8a1u, a81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/**
 * @brief Transforma o estado SHA-256 usando instruções de hardware ARMv8.
 */
void transform_arm_sha2(uint32_t* state, const uint8_t* data) {
    uint32x4_t abcd = vld1q_u32(state);
    uint32x4_t efgh = vld1q_u32(state + 4);
    
    // Carregar dados e inverter endianness
    uint32x4_t w0 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data)));
    uint32x4_t w1 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 16)));
    uint32x4_t w2 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 32)));
    uint32x4_t w3 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 48)));

    uint32x4_t msg0, msg1, msg2, msg3;
    uint32x4_t abcd_old = abcd;
    uint32x4_t efgh_old = efgh;

    // Rounds 0-3
    msg0 = vaddq_u32(w0, vld1q_u32(&kSHA256Table[0]));
    abcd = vsha256hq_u32(abcd, efgh, msg0);
    efgh = vsha256h2q_u32(efgh, abcd_old, msg0);
    
    // Rounds 4-7
    msg1 = vaddq_u32(w1, vld1q_u32(&kSHA256Table[4]));
    abcd_old = abcd;
    abcd = vsha256hq_u32(abcd, efgh, msg1);
    efgh = vsha256h2q_u32(efgh, abcd_old, msg1);

    // Rounds 8-11
    msg2 = vaddq_u32(w2, vld1q_u32(&kSHA256Table[8]));
    abcd_old = abcd;
    abcd = vsha256hq_u32(abcd, efgh, msg2);
    efgh = vsha256h2q_u32(efgh, abcd_old, msg2);

    // Rounds 12-15
    msg3 = vaddq_u32(w3, vld1q_u32(&kSHA256Table[12]));
    abcd_old = abcd;
    abcd = vsha256hq_u32(abcd, efgh, msg3);
    efgh = vsha256h2q_u32(efgh, abcd_old, msg3);

    // Message Expansion & Rounds 16-63
    for (int i = 16; i < 64; i += 16) {
        // ... (Loop desenrolado para expansão de mensagem e rodadas)
        // Por brevidade e performance, usaremos as instruções vsha256su0 e vsha256su1
        w0 = vsha256su1q_u32(vsha256su0q_u32(w0, w1), w2, w3);
        msg0 = vaddq_u32(w0, vld1q_u32(&kSHA256Table[i]));
        abcd_old = abcd;
        abcd = vsha256hq_u32(abcd, efgh, msg0);
        efgh = vsha256h2q_u32(efgh, abcd_old, msg0);
        
        w1 = vsha256su1q_u32(vsha256su0q_u32(w1, w2), w3, w0);
        msg1 = vaddq_u32(w1, vld1q_u32(&kSHA256Table[i+4]));
        abcd_old = abcd;
        abcd = vsha256hq_u32(abcd, efgh, msg1);
        efgh = vsha256h2q_u32(efgh, abcd_old, msg1);
        
        w2 = vsha256su1q_u32(vsha256su0q_u32(w2, w3), w0, w1);
        msg2 = vaddq_u32(w2, vld1q_u32(&kSHA256Table[i+8]));
        abcd_old = abcd;
        abcd = vsha256hq_u32(abcd, efgh, msg2);
        efgh = vsha256h2q_u32(efgh, abcd_old, msg2);
        
        w3 = vsha256su1q_u32(vsha256su0q_u32(w3, w0), w1, w2);
        msg3 = vaddq_u32(w3, vld1q_u32(&kSHA256Table[i+12]));
        abcd_old = abcd;
        abcd = vsha256hq_u32(abcd, efgh, msg3);
        efgh = vsha256h2q_u32(efgh, abcd_old, msg3);
    }

    // Acumular estado final
    abcd = vaddq_u32(abcd, vld1q_u32(state));
    efgh = vaddq_u32(efgh, vld1q_u32(state + 4));
    
    vst1q_u32(state, abcd);
    vst1q_u32(state + 4, efgh);
}

} // namespace bchaves::core

#endif
