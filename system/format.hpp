/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições e interfaces para utilitários de formatação.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "system/types.hpp"
#include "core/address.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace bchaves::system {

std::string format_quantity(double value, const char* const* units, std::size_t unit_count);
std::string format_key_count(double value);
std::string format_rate(double value);
std::string format_duration(std::uint64_t seconds);
std::string make_progress_bar(double ratio, std::size_t width = 30);

/**
 * @brief Exibe um relatório visual de sucesso no terminal.
 */
void print_success_report(const bchaves::core::DerivedKeyInfo& info, const std::string& context);

}  // namespace bchaves::system
