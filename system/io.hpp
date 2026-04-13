/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Utilitários de E/S para persistência de descobertas e logs.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "core/address.hpp"
#include <string>

namespace bchaves::system {

/**
 * @brief Salva um resultado encontrado em um arquivo (found.txt).
 * Garante thread-safety e persistência física via flush.
 */
bool save_found_result(const bchaves::core::DerivedKeyInfo& info, const std::string& context);

}  // namespace bchaves::system
