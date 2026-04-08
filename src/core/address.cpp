#include "core/address.hpp"

#include "core/base58.hpp"
#include "core/hash.hpp"
#include "core/ripemd160.hpp"

namespace qchaves::core {
namespace {

std::array<std::uint8_t, 32> bigint_to_bytes32(const BigInt& value) {
    std::array<std::uint8_t, 32> out{};
    for (std::size_t i = 0; i < value.limbs.size(); ++i) {
        const std::uint32_t limb = value.limbs[i];
        const std::size_t base = 31u - (i * 4u);
        out[base] = static_cast<std::uint8_t>(limb & 0xffu);
        out[base - 1u] = static_cast<std::uint8_t>((limb >> 8u) & 0xffu);
        out[base - 2u] = static_cast<std::uint8_t>((limb >> 16u) & 0xffu);
        out[base - 3u] = static_cast<std::uint8_t>((limb >> 24u) & 0xffu);
    }
    return out;
}

}  // namespace

std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.resize(bytes.size() * 2u);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2u] = kDigits[(bytes[i] >> 4u) & 0x0f];
        out[i * 2u + 1u] = kDigits[bytes[i] & 0x0f];
    }
    return out;
}

std::vector<std::uint8_t> hash160(const std::vector<std::uint8_t>& data) {
    const auto sha = sha256(data);
    const auto ripe = ripemd160(sha.data(), sha.size());
    return std::vector<std::uint8_t>(ripe.begin(), ripe.end());
}

std::vector<std::uint8_t> bitcoin_address_payload(const std::vector<std::uint8_t>& pubkey) {
    std::vector<std::uint8_t> payload;
    const auto h160 = hash160(pubkey);
    payload.reserve(25);
    payload.push_back(0x00);
    payload.insert(payload.end(), h160.begin(), h160.end());
    const auto checksum = double_sha256(payload);
    payload.insert(payload.end(), checksum.begin(), checksum.begin() + 4);
    return payload;
}

std::string bitcoin_address_string(const std::vector<std::uint8_t>& payload) {
    return base58_encode(payload);
}

std::string bitcoin_wif_string(const BigInt& private_key, bool compressed) {
    std::vector<std::uint8_t> payload;
    payload.reserve(compressed ? 38u : 37u);
    payload.push_back(0x80u);
    const auto private_key_bytes = bigint_to_bytes32(private_key);
    payload.insert(payload.end(), private_key_bytes.begin(), private_key_bytes.end());
    if (compressed) {
        payload.push_back(0x01u);
    }
    const auto checksum = double_sha256(payload);
    payload.insert(payload.end(), checksum.begin(), checksum.begin() + 4);
    return base58_encode(payload);
}

bool derive_key_info(const BigInt& private_key, DerivedKeyInfo& out) {
    const auto point = secp256k1_multiply(private_key);
    if (point.infinity) {
        return false;
    }

    out.private_key = private_key;
    out.pubkey_compressed = serialize_pubkey(point, true);
    out.pubkey_uncompressed = serialize_pubkey(point, false);
    out.pubkey_compressed_hex = bytes_to_hex(out.pubkey_compressed);
    out.pubkey_uncompressed_hex = bytes_to_hex(out.pubkey_uncompressed);
    out.address_payload_compressed = bitcoin_address_payload(out.pubkey_compressed);
    out.address_payload_uncompressed = bitcoin_address_payload(out.pubkey_uncompressed);
    out.address_compressed = bitcoin_address_string(out.address_payload_compressed);
    out.address_uncompressed = bitcoin_address_string(out.address_payload_uncompressed);
    out.wif_compressed = bitcoin_wif_string(private_key, true);
    out.wif_uncompressed = bitcoin_wif_string(private_key, false);
    return true;
}

}  // namespace qchaves::core
