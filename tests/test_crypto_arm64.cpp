/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Testes de validação para kernels ARM64 (SHA256 & RIPEMD160).
 */
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include "core/hash.hpp"
#include "core/ripemd160.hpp"

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

using namespace bchaves::core;

void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    std::cout << std::dec << std::endl;
}

bool verify_sha256() {
    std::cout << "[*] Testando SHA256 (ARM Hardware Acceleration if available)..." << std::endl;
    
    // Vector: "abc"
    const char* input = "abc";
    const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    
    Sha256 hash;
    hash.update((const uint8_t*)input, 3);
    auto result = hash.finalize();
    
    if (std::memcmp(result.data(), expected, 32) == 0) {
        std::cout << "[OK] SHA256 validado." << std::endl;
        return true;
    } else {
        std::cout << "[FAIL] SHA256 incorreto!" << std::endl;
        std::cout << "  Esperado: "; print_hex(expected, 32);
        std::cout << "  Obtido:   "; print_hex(result.data(), 32);
        return false;
    }
}

bool verify_ripemd160() {
    std::cout << "[*] Testando RIPEMD160 (NEON Vectorization if available)..." << std::endl;
    
    // Vector: "abc"
    const char* input = "abc";
    const uint8_t expected[20] = {
        0x8e, 0xb2, 0x08, 0xf7, 0xe0, 0x5d, 0x98, 0x7a, 0x9b, 0x04, 0x4a, 0x8e, 0x98, 0xc6, 0xb0, 0x87,
        0xf1, 0x5a, 0x0b, 0xfc
    };
    
    auto result = ripemd160((const uint8_t*)input, 3);
    
    if (std::memcmp(result.data(), expected, 20) == 0) {
        std::cout << "[OK] RIPEMD160 validado." << std::endl;
        return true;
    } else {
        std::cout << "[FAIL] RIPEMD160 incorreto!" << std::endl;
        std::cout << "  Esperado: "; print_hex(expected, 20);
        std::cout << "  Obtido:   "; print_hex(result.data(), 20);
        return false;
    }
}

int main() {
    std::cout << "=== Bchaves: ARM64 Crypto Kernel Test Suite ===" << std::endl;
    
#if !defined(__aarch64__)
    std::cout << "[INFO] Executando em arquitetura não-ARM64. Testando dispatch para fallback/scalar." << std::endl;
#endif

    bool success = true;
    success &= verify_sha256();
    success &= verify_ripemd160();

    if (success) {
        std::cout << "\n[SUCCESS] Todos os kernels criptográficos foram validados!" << std::endl;
        return 0;
    } else {
        std::cout << "\n[ERROR] Falha na validação dos kernels." << std::endl;
        return 1;
    }
}
