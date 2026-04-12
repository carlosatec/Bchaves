#include "system/targets.hpp"

#include "core/base58.hpp"
#include "core/hash.hpp"

#include <fstream>
#include <utility>

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
    return raw.find('/') != std::string::npos ||
           raw.find('\\') != std::string::npos ||
           input.has_extension();
}

}  // namespace

TargetType detect_type(const std::string& line) {
    if (line.size() == 66u && bchaves::core::is_hex(line) && (line.rfind("02", 0) == 0 || line.rfind("03", 0) == 0)) {
        return TargetType::pubkey_compress;
    }
    if (line.size() == 130u && bchaves::core::is_hex(line) && line.rfind("04", 0) == 0) {
        return TargetType::pubkey_uncompress;
    }
    if (line.size() >= 26u && line.size() <= 35u && bchaves::core::is_base58_string(line)) {
        return TargetType::address_btc;
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
    if (require_pubkeys_only && type == TargetType::address_btc) {
        result.warnings.push_back({0, "modo exige public keys, endereco BTC inline ignorado"});
        return result;
    }

    TargetEntry entry;
    entry.line_number = 1;
    entry.raw = cleaned;
    entry.type = type;

    if (type == TargetType::address_btc) {
        bool ok = false;
        const auto decoded = bchaves::core::base58_decode(cleaned, &ok);
        if (!ok || !validate_address_payload(decoded)) {
            result.warnings.push_back({0, "checksum invalido em alvo inline"});
            return result;
        }
        entry.payload.assign(decoded.begin(), decoded.end());
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
        if (require_pubkeys_only && type == TargetType::address_btc) {
            result.warnings.push_back({line_number, "modo exige public keys, endereco BTC ignorado"});
            continue;
        }

        TargetEntry entry;
        entry.line_number = line_number;
        entry.raw = cleaned;
        entry.type = type;

        if (type == TargetType::address_btc) {
            bool ok = false;
            const auto decoded = bchaves::core::base58_decode(cleaned, &ok);
            if (!ok || !validate_address_payload(decoded)) {
                result.warnings.push_back({line_number, "checksum invalido"});
                continue;
            }
            entry.payload.assign(decoded.begin(), decoded.end());
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
