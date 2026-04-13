/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições para codificação Base58.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bchaves::core {

std::vector<std::uint8_t> base58_decode(const std::string& input, bool* ok = nullptr);
std::string base58_encode(const std::vector<std::uint8_t>& input);
bool is_base58_string(const std::string& input);

}  // namespace bchaves::core
