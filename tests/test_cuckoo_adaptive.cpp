/*
 * Bchaves: Testes para AdaptiveCuckooFilter
 */
#include "core/adaptive_filter.hpp"
#include "system/hardware.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

using namespace bchaves::core;
using namespace bchaves::system;

void test_basic_ops() {
    std::cout << "[*] Testando operações básicas...\n";
    HardwareInfo hw = detect_hardware();
    AdaptiveCuckooFilter filter(1000, hw);

    uint8_t target[20] = {0x01, 0x02, 0x03};
    for(int i=3; i<20; ++i) target[i] = i;
    
    filter.insert(target);

    if (filter.lookup(target) != true) {
        std::cerr << "[FAIL] Fingerprint inserido não encontrado!\n";
        exit(1);
    }
    
    uint8_t missing[20];
    std::memset(missing, 0xFF, 20);
    if (filter.lookup(missing) != false) {
        std::cerr << "[FAIL] Encontrou item não inserido (falso positivo inesperado em set pequeno)!\n";
        exit(1);
    }
    
    std::cout << "[OK] Inserção e busca básica.\n";
}

void test_batch_ops() {
    std::cout << "[*] Testando busca em lote (SIMD Dispatch)...\n";
    HardwareInfo hw = detect_hardware();
    AdaptiveCuckooFilter filter(1000, hw);

    uint8_t batch[8][20];
    bool expected[8];
    for(int i=0; i<8; ++i) {
        std::memset(batch[i], i + 1, 20);
        if (i % 2 == 0) {
            filter.insert(batch[i]);
            expected[i] = true;
        } else {
            expected[i] = false;
        }
    }

    bool results[8];
    filter.lookup_batch(batch[0], results, 8);

    for(int i=0; i<8; ++i) {
        if (results[i] != expected[i]) {
            std::cerr << "[FAIL] Resultado do lote no índice " << i << " incorreto!\n";
            std::cerr << "       Esperado: " << (expected[i] ? "TRUE" : "FALSE") 
                      << " | Obtido: " << (results[i] ? "TRUE" : "FALSE") << "\n";
            exit(1);
        }
    }
    
    std::cout << "[OK] Busca em lote.\n";
}

int main() {
    std::cout << "=== Bchaves: AdaptiveCuckooFilter Test Suite ===\n";
    test_basic_ops();
    test_batch_ops();
    std::cout << "\n[SUCCESS] Todos os testes do AdaptiveCuckooFilter passaram!\n";
    return 0;
}
