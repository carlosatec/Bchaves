/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Codificação e decodificação Base58 (usado em endereços P2PKH/WIF).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "core/base58.hpp"

#include <cstring>

namespace bchaves::core {
namespace {

constexpr char kAlphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

int alphabet_index(char ch) {
    const char* found = std::strchr(kAlphabet, ch);
    return found == nullptr ? -1 : static_cast<int>(found - kAlphabet);
}

}  // namespace

bool is_base58_string(const std::string& input) {
    if (input.empty()) {
        return false;
    }
    for (const char ch : input) {
        if (alphabet_index(ch) < 0) {
            return false;
        }
    }
    return true;
}

std::string base58_encode(const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        return {};
    }

    std::vector<std::uint8_t> digits((input.size() * 138u) / 100u + 1u, 0u);
    std::size_t digits_length = 1u;

    for (const std::uint8_t byte : input) {
        int carry = byte;
        for (std::size_t i = 0; i < digits_length; ++i) {
            const int value = static_cast<int>(digits[i]) * 256 + carry;
            digits[i] = static_cast<std::uint8_t>(value % 58);
            carry = value / 58;
        }
        while (carry > 0) {
            digits[digits_length++] = static_cast<std::uint8_t>(carry % 58);
            carry /= 58;
        }
    }

    std::size_t zero_prefix = 0;
    while (zero_prefix < input.size() && input[zero_prefix] == 0u) {
        ++zero_prefix;
    }

    std::string result(zero_prefix, '1');
    for (std::size_t i = 0; i < digits_length; ++i) {
        result.push_back(kAlphabet[digits[digits_length - 1u - i]]);
    }
    return result;
}

std::vector<std::uint8_t> base58_decode(const std::string& input, bool* ok) {
    if (ok != nullptr) {
        *ok = false;
    }
    if (!is_base58_string(input)) {
        return {};
    }

    std::vector<std::uint8_t> output((input.size() * 733u) / 1000u + 1u, 0u);
    std::size_t output_length = 1u;

    for (const char ch : input) {
        const int carry_input = alphabet_index(ch);
        if (carry_input < 0) {
            return {};
        }
        int carry = carry_input;
        for (std::size_t i = 0; i < output_length; ++i) {
            const int value = static_cast<int>(output[i]) * 58 + carry;
            output[i] = static_cast<std::uint8_t>(value & 0xff);
            carry = value >> 8;
        }
        while (carry > 0) {
            output[output_length++] = static_cast<std::uint8_t>(carry & 0xff);
            carry >>= 8;
        }
    }

    std::size_t zero_prefix = 0;
    while (zero_prefix < input.size() && input[zero_prefix] == '1') {
        ++zero_prefix;
    }

    std::vector<std::uint8_t> result(zero_prefix + output_length, 0u);
    for (std::size_t i = 0; i < output_length; ++i) {
        result[result.size() - 1u - i] = output[i];
    }

    if (ok != nullptr) {
        *ok = true;
    }
    return result;
}

}  // namespace bchaves::core
