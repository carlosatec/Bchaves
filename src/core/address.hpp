#pragma once

#include "core/secp256k1.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace qchaves::core {

struct DerivedKeyInfo {
    BigInt private_key{};
    std::vector<std::uint8_t> pubkey_compressed;
    std::vector<std::uint8_t> pubkey_uncompressed;
    std::vector<std::uint8_t> address_payload_compressed;
    std::vector<std::uint8_t> address_payload_uncompressed;
    std::string pubkey_compressed_hex;
    std::string pubkey_uncompressed_hex;
    std::string address_compressed;
    std::string address_uncompressed;
    std::string wif_compressed;
    std::string wif_uncompressed;
};

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes);
std::vector<std::uint8_t> hash160(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> bitcoin_address_payload(const std::vector<std::uint8_t>& pubkey);
std::string bitcoin_address_string(const std::vector<std::uint8_t>& payload);
std::string bitcoin_wif_string(const BigInt& private_key, bool compressed);
bool derive_key_info(const BigInt& private_key, DerivedKeyInfo& out);

}  // namespace qchaves::core
