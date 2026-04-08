#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qchaves::core {

std::vector<std::uint8_t> base58_decode(const std::string& input, bool* ok = nullptr);
std::string base58_encode(const std::vector<std::uint8_t>& input);
bool is_base58_string(const std::string& input);

}  // namespace qchaves::core
