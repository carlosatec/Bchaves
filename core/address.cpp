#include "core/address.hpp"

#include "core/hash.hpp"
#include "core/ripemd160.hpp"

namespace bchaves::core {
namespace {

std::string to_hex_string(const ByteVector& data) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (const std::uint8_t byte : data) {
        out += hex[(byte >> 4u) & 0xfu];
        out += hex[byte & 0xfu];
    }
    return out;
}

ByteVector to_wif(const BigInt& private_key, bool compressed) {
    ByteVector key_bytes;
    key_bytes.reserve(compressed ? 34 : 33);
    
    // Serializar BigInt (256 bits / 32 bytes)
    for (int i = 7; i >= 0; --i) {
        std::uint32_t limb = private_key.limbs[i];
        key_bytes.push_back(static_cast<std::uint8_t>((limb >> 24) & 0xff));
        key_bytes.push_back(static_cast<std::uint8_t>((limb >> 16) & 0xff));
        key_bytes.push_back(static_cast<std::uint8_t>((limb >> 8) & 0xff));
        key_bytes.push_back(static_cast<std::uint8_t>(limb & 0xff));
    }

    if (compressed) {
        key_bytes.push_back(0x01);
    }
    return key_bytes;
}

ByteVector hash_pubkey(const Secp256k1Point& point, bool compressed) {
    const auto serialized = serialize_pubkey(point, compressed);
    const auto sha = sha256(serialized.data(), serialized.size());
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

    const auto serialized_compressed = serialize_pubkey(point, true);
    out.pubkey_compressed_hex = to_hex_string(serialized_compressed);

    const auto serialized_uncompressed = serialize_pubkey(point, false);
    out.pubkey_uncompressed_hex = to_hex_string(serialized_uncompressed);

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