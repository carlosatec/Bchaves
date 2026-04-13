/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de derivação de endereços Bitcoin (Legacy, P2SH, SegWit).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "core/address.hpp"

#include "core/hash.hpp"
#include "core/ripemd160.hpp"

namespace bchaves::core {
namespace {

std::string to_hex_string(const std::uint8_t* data, std::size_t length) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        out += hex[(data[i] >> 4u) & 0xfu];
        out += hex[data[i] & 0xfu];
    }
    return out;
}

ByteVector to_wif(const BigInt& private_key, bool compressed) {
    ByteVector key_bytes;
    key_bytes.reserve(compressed ? 34 : 33);
    
    // Serializar BigInt (256 bits / 32 bytes) - Big Endian para WIF
    for (int i = 3; i >= 0; --i) {
        std::uint64_t limb = private_key.limbs[i];
        for (int j = 7; j >= 0; --j) {
            key_bytes.push_back(static_cast<std::uint8_t>((limb >> (j * 8)) & 0xff));
        }
    }

    if (compressed) {
        key_bytes.push_back(0x01);
    }
    return key_bytes;
}

ByteVector hash_pubkey(const Secp256k1Point& point, bool compressed) {
    std::uint8_t buf[65];
    std::size_t len = serialize_pubkey(point, compressed, buf);
    const auto sha = sha256(buf, len);
    const auto hash = ripemd160(sha.data(), sha.size());
    ByteVector out(hash.begin(), hash.end());
    return out;
}

ByteVector hash_payload(const ByteVector& payload, std::uint8_t version) {
    ByteVector versioned;
    versioned.reserve(payload.size() + 1);
    versioned.push_back(version);
    versioned.insert(versioned.end(), payload.begin(), payload.end());
    const auto sha = sha256(versioned.data(), versioned.size());
    const auto digest = sha256(sha.data(), sha.size());
    ByteVector result;
    result.reserve(payload.size() + 1);
    result.push_back(version);
    result.insert(result.end(), payload.begin(), payload.end());
    result.insert(result.end(), digest.begin(), digest.begin() + 4);
    return result;
}

}  // namespace

bool derive_key_info(const BigInt& private_key, DerivedKeyInfo& out) {
    if (!is_valid_private_key(private_key)) {
        return false;
    }

    out.private_key = private_key;

    const auto point = secp256k1_multiply(private_key);
    if (point.infinity) {
        return false;
    }

    std::uint8_t buf[65];
    std::size_t len_c = serialize_pubkey(point, true, buf);
    out.pubkey_compressed_hex = to_hex_string(buf, len_c);

    std::size_t len_u = serialize_pubkey(point, false, buf);
    out.pubkey_uncompressed_hex = to_hex_string(buf, len_u);

    const auto hash_compressed = hash_pubkey(point, true);
    out.address_payload_compressed = hash_compressed;

    const auto hash_uncompressed = hash_pubkey(point, false);
    out.address_payload_uncompressed = hash_uncompressed;

    out.address_compressed = base58_encode(hash_payload(hash_compressed, 0x00));
    out.address_uncompressed = base58_encode(hash_payload(hash_uncompressed, 0x00));

    const auto wif_compressed = to_wif(private_key, true);
    out.wif_compressed = base58_encode(wif_compressed);

    const auto wif_uncompressed = to_wif(private_key, false);
    out.wif_uncompressed = base58_encode(wif_uncompressed);

    return true;
}

}  // namespace bchaves::core