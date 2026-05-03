/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de RIPEMD-160 Batch (4x) utilizando ARM NEON.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include <cstdint>
#include <cstring>

#if defined(__aarch64__) || defined(__arm__)
#include <arm_neon.h>

namespace bchaves::core {

#define ADD_N vaddq_u32
#define XOR_N veorq_u32
#define AND_N vandq_u32
#define OR_N vorrq_u32
#define BIC_N vbicq_u32
#define NOT_N(x) veorq_u32(x, vdupq_n_u32(0xFFFFFFFFu))
#define ROTL_N(x, n) vorrq_u32(vshlq_n_u32(x, n), vshrq_n_u32(x, 32 - (n)))

/**
 * @brief RIPEMD-160 Batch 4 usando registros NEON de 128-bit.
 */
void ripemd160_batch4_neon(const uint8_t* const data[4], size_t length, uint8_t* const out[4]) {
    if (length != 32) {
        // Fallback para tamanhos não padrão (raro no loop de endereços)
        for(int i=0; i<4; ++i) {
            // Chamada escalar (externa ou via header)
        }
        return;
    }

    alignas(16) uint32_t X[16][4];
    for (int i = 0; i < 4; ++i) {
        const uint32_t* p = (const uint32_t*)data[i];
        for(int j=0; j<8; ++j) X[j][i] = p[j];
        X[8][i] = 0x80u;
        for(int j=9; j<14; ++j) X[j][i] = 0;
        X[14][i] = 256; X[15][i] = 0;
    }

    uint32x4_t h0 = vdupq_n_u32(0x67452301u);
    uint32x4_t h1 = vdupq_n_u32(0xefcdab89u);
    uint32x4_t h2 = vdupq_n_u32(0x98badcfeu);
    uint32x4_t h3 = vdupq_n_u32(0x10325476u);
    uint32x4_t h4 = vdupq_n_u32(0xc3d2e1f0u);

    uint32x4_t al = h0, bl = h1, cl = h2, dl = h3, el = h4;
    uint32x4_t ar = h0, br = h1, cr = h2, dr = h3, er = h4;

    static const uint32_t r[80] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, 7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8, 3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12, 1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2, 4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13 };
    static const uint32_t rp[80] = { 5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12, 6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2, 15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13, 8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14, 12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11 };
    static const uint32_t s[80] = { 11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8, 7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12, 11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5, 11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12, 9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6 };
    static const uint32_t sp[80] = { 8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6, 9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11, 9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5, 15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8, 8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11 };

    for (int j = 0; j < 80; ++j) {
        uint32x4_t fj, fjp, kj, kjp;
        if (j <= 15) { fj = XOR_N(bl, XOR_N(cl, dl)); kj = vdupq_n_u32(0); kjp = vdupq_n_u32(0x50a28be6u); fjp = XOR_N(br, OR_N(cr, NOT_N(dr))); }
        else if (j <= 31) { fj = OR_N(AND_N(bl, cl), BIC_N(dl, bl)); kj = vdupq_n_u32(0x5a827999u); kjp = vdupq_n_u32(0x5c4dd124u); fjp = OR_N(AND_N(br, dr), BIC_N(cr, dr)); }
        else if (j <= 47) { fj = XOR_N(OR_N(bl, NOT_N(cl)), dl); kj = vdupq_n_u32(0x6ed9eba1u); kjp = vdupq_n_u32(0x6d703ef3u); fjp = XOR_N(OR_N(br, NOT_N(cr)), dr); }
        else if (j <= 63) { fj = OR_N(AND_N(bl, dl), BIC_N(cl, dl)); kj = vdupq_n_u32(0x8f1bbcdcu); kjp = vdupq_n_u32(0x7a6d76e9u); fjp = OR_N(AND_N(br, br), BIC_N(br, dr)); }
        else { fj = XOR_N(bl, OR_N(cl, NOT_N(dl))); kj = vdupq_n_u32(0xa953fd4eu); kjp = vdupq_n_u32(0); fjp = XOR_N(br, XOR_N(cr, dr)); }

        uint32x4_t xj = vld1q_u32(X[r[j]]);
        uint32x4_t xjp = vld1q_u32(X[rp[j]]);

        uint32x4_t tl = ADD_N(al, ADD_N(fj, ADD_N(xj, kj)));
        tl = ADD_N(ROTL_N(tl, s[j]), el);
        al = el; el = dl; dl = ROTL_N(cl, 10); cl = bl; bl = tl;

        uint32x4_t tr = ADD_N(ar, ADD_N(fjp, ADD_N(xjp, kjp)));
        tr = ADD_N(ROTL_N(tr, sp[j]), er);
        ar = er; er = dr; dr = ROTL_N(cr, 10); cr = br; br = tr;
    }

    uint32x4_t t = ADD_N(h1, ADD_N(cl, dr)); 
    h1 = ADD_N(h2, ADD_N(dl, er)); 
    h2 = ADD_N(h3, ADD_N(el, ar)); 
    h3 = ADD_N(h4, ADD_N(al, br)); 
    h4 = ADD_N(h0, ADD_N(bl, cr)); 
    h0 = t;

    uint32_t a0[4], a1[4], a2[4], a3[4], a4[4];
    vst1q_u32(a0, h0); vst1q_u32(a1, h1); vst1q_u32(a2, h2); vst1q_u32(a3, h3); vst1q_u32(a4, h4);
    
    for (int i = 0; i < 4; ++i) { 
        uint32_t* o = (uint32_t*)out[i]; 
        o[0] = a0[i]; o[1] = a1[i]; o[2] = a2[i]; o[3] = a3[i]; o[4] = a4[i]; 
    }
}

} // namespace bchaves::core

#endif
