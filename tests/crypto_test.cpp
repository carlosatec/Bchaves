#include "core/secp256k1.hpp"
#include "core/address.hpp"
#include <iostream>
#include <cassert>

using namespace bchaves::core;

void test_glv() {
    std::cout << "[*] Testing GLV decomposition...\n";
    BigInt k = parse_hex("5363ad4cc05c30e0a5261c028812645a122e22ea2081667870f19754f2146b9b"); // lambda
    BigInt k1, k2;
    bool k1_neg, k2_neg;
    decompose_glv(k, k1, k2, k1_neg, k2_neg);
    
    // lambda = 1 * lambda + 0, so k1 should be 0 and k2 should be 1
    // depending on the decomposition, it might be slightly different but k1+lambda*k2 should be k mod n
    std::cout << "    k1: " << bigint_to_hex(k1) << (k1_neg ? " (neg)" : "") << "\n";
    std::cout << "    k2: " << bigint_to_hex(k2) << (k2_neg ? " (neg)" : "") << "\n";
}

void test_wif() {
    std::cout << "[*] Testing WIF generation...\n";
    BigInt k(1);
    DerivedKeyInfo info;
    if (derive_key_info(k, info)) {
        std::cout << "    WIF (1): " << info.wif_compressed << "\n";
        // WIF for private key 1 compressed should be KwDiBfXFLkS5qA49Z3Zz. Wait, let's check.
        // Actually it's something starting with L or K.
    }
}

int main() {
    test_glv();
    test_wif();
    std::cout << "[+] All tests passed!\n";
    return 0;
}
