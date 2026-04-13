/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Carregador de alvos polimórfico (Addresses e Pubkeys).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "system/targets.hpp"

#include "core/base58.hpp"
#include "core/hash.hpp"

#include <fstream>
#include <utility>
#include <cstring>

namespace bchaves::system {
namespace {

std::string trim(std::string value) {
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(start, end - start);
}

bool validate_address_payload(const std::vector<std::uint8_t>& decoded) {
    if (decoded.size() != 25u) {
        return false;
    }
    bchaves::core::ByteVector body(decoded.begin(), decoded.begin() + 21);
    const auto digest = bchaves::core::double_sha256(body);
    return digest[0] == decoded[21] && digest[1] == decoded[22] && digest[2] == decoded[23] && digest[3] == decoded[24];
}

bool validate_address_p2sh(const std::vector<std::uint8_t>& decoded) {
    if (decoded.size() != 25u || decoded[0] != 0x05u) return false;
    bchaves::core::ByteVector body(decoded.begin(), decoded.begin() + 21);
    const auto digest = bchaves::core::double_sha256(body);
    return digest[0] == decoded[21] && digest[1] == decoded[22] && digest[2] == decoded[23] && digest[3] == decoded[24];
}

std::vector<uint8_t> decode_bech32_hash(const std::string& addr) {
    // Implementação simplificada para extrair o Hash160 de endereços bc1q... (v0)
    static const char* charset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
    if (addr.size() < 42 || addr.substr(0, 3) != "bc1") return {};
    
    // Pula o HRP "bc1" e o separador '1'
    std::string data = addr.substr(4);
    std::vector<uint8_t> values;
    for (char c : data) {
        const char* p = strchr(charset, c);
        if (!p) return {};
        values.push_back(p - charset);
    }
    
    // Bech32 v0 bits conversion (32 to 256 for extraction of 20 bytes)
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    for (uint8_t v : values) {
        acc = (acc << 5) | v;
        bits += 5;
        while (bits >= 8) {
            bits -= 8;
            out.push_back((acc >> bits) & 0xFF);
        }
    }
    // Para P2WPKH, o primeiro byte é a versão (0) e o segundo é o tamanho (20)
    // Então o Hash160 começa no índice 2 e tem 20 bytes
    if (out.size() >= 22 && out[0] == 0) {
        return std::vector<uint8_t>(out.begin() + 2, out.begin() + 22);
    }
    return {};
}

bool is_valid_pubkey(TargetType type, const std::string& line) {
    if (!bchaves::core::is_hex(line)) {
        return false;
    }
    if (type == TargetType::pubkey_compress) {
        return line.size() == 66u && (line.rfind("02", 0) == 0 || line.rfind("03", 0) == 0);
    }
    if (type == TargetType::pubkey_uncompress) {
        return line.size() == 130u && line.rfind("04", 0) == 0;
    }
    return false;
}

bool looks_like_file_reference(const std::filesystem::path& input) {
    const std::string raw = input.string();
    if (raw.size() <= 1) return false;
    if (raw.find('/') != std::string::npos || raw.find('\\') != std::string::npos) return true;
    if (input.has_extension()) return true;
    if (raw.size() >= 26 && raw.size() <= 35 && raw.find('.') == std::string::npos) return false;
    return false;
}

}  // namespace

TargetType detect_type(const std::string& line) {
    if (line.size() == 66u && bchaves::core::is_hex(line) && (line.rfind("02", 0) == 0 || line.rfind("03", 0) == 0)) {
        return TargetType::pubkey_compress;
    }
    if (line.size() == 130u && bchaves::core::is_hex(line) && line.rfind("04", 0) == 0) {
        return TargetType::pubkey_uncompress;
    }
    if (line.size() == 40u && bchaves::core::is_hex(line)) {
        return TargetType::hash160;
    }
    if (line.size() >= 26u && line.size() <= 35u && bchaves::core::is_base58_string(line)) {
        if (line[0] == '1') return TargetType::address_btc;
        if (line[0] == '3') return TargetType::address_p2sh;
    }
    if (line.size() >= 42u && line.rfind("bc1", 0) == 0) {
        return TargetType::address_bech32;
    }
    return TargetType::invalid;
}

TargetLoadResult load_target_inline(const std::string& input, bool require_pubkeys_only) {
    TargetLoadResult result;
    const std::string cleaned = trim(input);
    if (cleaned.empty()) {
        result.warnings.push_back({0, "alvo inline vazio"});
        return result;
    }

    const TargetType type = detect_type(cleaned);
    if (type == TargetType::invalid) {
        result.warnings.push_back({0, "alvo inline invalido"});
        return result;
    }
    if (require_pubkeys_only && (type == TargetType::address_btc || type == TargetType::address_p2sh || type == TargetType::address_bech32)) {
        result.warnings.push_back({0, "modo exige public keys, endereco ignorado em alvo inline"});
        return result;
    }

    TargetEntry entry;
    entry.line_number = 1;
    entry.raw = cleaned;
    entry.type = type;

    if (type == TargetType::address_btc || type == TargetType::address_p2sh) {
        bool ok = false;
        const auto decoded = bchaves::core::base58_decode(cleaned, &ok);
        if (!ok || (type == TargetType::address_btc && !validate_address_payload(decoded)) || 
            (type == TargetType::address_p2sh && !validate_address_p2sh(decoded))) {
            result.warnings.push_back({0, "checksum ou versao invalida em alvo inline"});
            return result;
        }
        // O payload para busca é o Hash160 (bytes 1 a 21)
        entry.payload.assign(decoded.begin() + 1, decoded.begin() + 21);
    } else if (type == TargetType::address_bech32) {
        auto hash = decode_bech32_hash(cleaned);
        if (hash.empty()) {
            result.warnings.push_back({0, "bech32 invalido ou nao suportado"});
            return result;
        }
        entry.payload = std::move(hash);
    } else if (type == TargetType::hash160) {
        entry.payload = bchaves::core::from_hex(cleaned);
    } else {
        if (!is_valid_pubkey(type, cleaned)) {
            result.warnings.push_back({0, "public key inline invalida"});
            return result;
        }
        entry.payload = bchaves::core::from_hex(cleaned);
    }

    result.entries.push_back(std::move(entry));
    return result;
}

TargetLoadResult load_targets(const std::filesystem::path& file, bool require_pubkeys_only) {
    TargetLoadResult result;
    std::ifstream in(file);
    if (!in) {
        if (looks_like_file_reference(file)) {
            result.warnings.push_back({0, "arquivo nao encontrado: " + file.string()});
            return result;
        }
        return load_target_inline(file.string(), require_pubkeys_only);
    }

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        const std::string cleaned = trim(line);
        if (cleaned.empty()) {
            continue;
        }
        const TargetType type = detect_type(cleaned);
        if (type == TargetType::invalid) {
            result.warnings.push_back({line_number, "formato invalido (nao e base58 nem pubkey hex)"});
            continue;
        }
        if (require_pubkeys_only && (type == TargetType::address_btc || type == TargetType::address_p2sh || type == TargetType::address_bech32)) {
            result.warnings.push_back({line_number, "modo exige public keys, endereco ignorado"});
            continue;
        }

        TargetEntry entry;
        entry.line_number = line_number;
        entry.raw = cleaned;
        entry.type = type;

        if (type == TargetType::address_btc || type == TargetType::address_p2sh) {
            bool ok = false;
            const auto decoded = bchaves::core::base58_decode(cleaned, &ok);
            if (!ok || (type == TargetType::address_btc && !validate_address_payload(decoded)) || 
                (type == TargetType::address_p2sh && !validate_address_p2sh(decoded))) {
                result.warnings.push_back({line_number, "checksum ou versao invalida"});
                continue;
            }
            entry.payload.assign(decoded.begin() + 1, decoded.begin() + 21);
        } else if (type == TargetType::address_bech32) {
            auto hash = decode_bech32_hash(cleaned);
            if (hash.empty()) {
                result.warnings.push_back({line_number, "bech32 invalido ou nao suportado"});
                continue;
            }
            entry.payload = std::move(hash);
        } else if (type == TargetType::hash160) {
            entry.payload = bchaves::core::from_hex(cleaned);
        } else {
            if (!is_valid_pubkey(type, cleaned)) {
                result.warnings.push_back({line_number, "public key invalida"});
                continue;
            }
            entry.payload = bchaves::core::from_hex(cleaned);
        }

        result.entries.push_back(std::move(entry));
    }
    return result;
}

}  // namespace bchaves::system
