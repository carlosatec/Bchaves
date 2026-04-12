#pragma once

#include <string>
#include <vector>

#include "core/secp256k1.hpp"
#include "core/base58.hpp"
#include "core/hash.hpp"

namespace bchaves::core {

struct DerivedKeyInfo {
    BigInt private_key;
    std::string wif_compressed;
    std::string wif_uncompressed;
    std::string pubkey_compressed_hex;
    std::string pubkey_uncompressed_hex;
    std::vector<std::uint8_t> address_payload_compressed;
    std::vector<std::uint8_t> address_payload_uncompressed;
    std::string address_compressed;
    std::string address_uncompressed;
};

bool derive_key_info(const BigInt& private_key, DerivedKeyInfo& out);

}  // namespace bchaves::core