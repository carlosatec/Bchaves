/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Utilitários de formatação de strings, durações e taxas.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "system/format.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace bchaves::system {
namespace {

constexpr const char* kKeyUnits[] = {"", "K", "M", "B", "T", "Q", "Qi"};
constexpr const char* kRateUnits[] = {"", "K", "M", "G", "P", "E", "Z", "Y"};

}  // namespace

std::string format_quantity(double value, const char* const* units, std::size_t unit_count) {
    std::size_t index = 0;
    while (value >= 1000.0 && index + 1 < unit_count) {
        value /= 1000.0;
        ++index;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(value < 10.0 ? 1 : 0) << value;
    if (units[index][0] != '\0') {
        out << ' ' << units[index];
    }
    return out.str();
}

std::string format_key_count(double value) {
    return format_quantity(value, kKeyUnits, sizeof(kKeyUnits) / sizeof(kKeyUnits[0]));
}

std::string format_rate(double value) {
    return format_quantity(value, kRateUnits, sizeof(kRateUnits) / sizeof(kRateUnits[0])) + "/s";
}

std::string format_duration(std::uint64_t seconds) {
    const std::uint64_t hours = seconds / 3600u;
    const std::uint64_t minutes = (seconds % 3600u) / 60u;
    const std::uint64_t secs = seconds % 60u;

    std::ostringstream out;
    if (hours > 0) {
        out << hours << 'h' << ' ';
    }
    if (minutes > 0 || hours > 0) {
        out << minutes << 'm';
        if (hours == 0) {
            out << ' ' << secs << 's';
        }
    } else {
        out << secs << 's';
    }
    return out.str();
}

std::string make_progress_bar(double ratio, std::size_t width) {
    const double clamped = std::max(0.0, std::min(1.0, ratio));
    const std::size_t fill = static_cast<std::size_t>(std::round(clamped * static_cast<double>(width)));
    return "[" + std::string(fill, '#') + std::string(width - fill, '.') + "]";
}

std::string to_string(TargetType value) {
    switch (value) {
        case TargetType::address_btc: return "address";
        case TargetType::pubkey_compress: return "pubkey-compress";
        case TargetType::pubkey_uncompress: return "pubkey-uncompress";
        default: return "invalid";
    }
}

std::string to_string(SearchMode value) {
    switch (value) {
        case SearchMode::sequential: return "sequential";
        case SearchMode::backward: return "backward";
        case SearchMode::both: return "both";
        case SearchMode::hybrid: return "hybrid";
        default: return "unknown";
    }
}

std::string to_string(SearchType value) {
    switch (value) {
        case SearchType::compress: return "compress";
        case SearchType::uncompress: return "uncompress";
        case SearchType::both: return "both";
        default: return "unknown";
    }
}

std::string to_string(AutoTuneProfile value) {
    switch (value) {
        case AutoTuneProfile::safe: return "safe";
        case AutoTuneProfile::balanced: return "balanced";
        case AutoTuneProfile::max: return "max";
        default: return "unknown";
    }
}

std::string to_string(Secp256k1BackendPreference value) {
    switch (value) {
        case Secp256k1BackendPreference::auto_select: return "auto";
        case Secp256k1BackendPreference::portable: return "portable";
        case Secp256k1BackendPreference::external: return "external";
        default: return "unknown";
    }
}

void print_success_report(const bchaves::core::DerivedKeyInfo& info, const std::string& context) {
    std::cout << "\n\033[92m================================================================================\033[0m\n";
    std::cout << "\033[92m                  Bchaves Performance Engine: Solution Detected                 \033[0m\n";
    std::cout << "\033[92m================================================================================\033[0m\n\n";
    
    std::cout << "\033[1;32m   ✓ SUCESSO! CHAVE ENCONTRADA\033[0m\n\n";
    
    std::cout << "\033[36m   Context       :\033[0m " << context << "\n";
    std::cout << "\033[36m   Private Hex   :\033[0m \033[93m0x" << bchaves::core::bigint_to_hex(info.private_key) << "\033[0m\n";
    std::cout << "\033[36m   Private Dec   :\033[0m " << bchaves::core::bigint_to_decimal(info.private_key) << "\n";
    std::cout << "\033[36m   WIF Format    :\033[0m \033[92m" << info.wif_compressed << "\033[0m\n";
    std::cout << "\033[36m   Address BTC   :\033[0m " << info.address_compressed << "\n\n";
    
    std::cout << "   Validado via ECDSA (bchaves-core)\n";
    std::cout << "\033[92m================================================================================\033[0m\n\n";
}

}  // namespace bchaves::system
