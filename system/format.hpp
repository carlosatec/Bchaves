#pragma once

#include "system/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace bchaves::system {

std::string format_quantity(double value, const char* const* units, std::size_t unit_count);
std::string format_key_count(double value);
std::string format_rate(double value);
std::string format_duration(std::uint64_t seconds);
std::string make_progress_bar(double ratio, std::size_t width = 30);

}  // namespace bchaves::system
