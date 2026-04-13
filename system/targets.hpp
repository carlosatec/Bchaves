/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições e interfaces para o carregador de alvos.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "system/types.hpp"

#include <filesystem>

namespace bchaves::system {

TargetType detect_type(const std::string& line);
TargetLoadResult load_targets(const std::filesystem::path& file, bool require_pubkeys_only = false);
TargetLoadResult load_target_inline(const std::string& input, bool require_pubkeys_only = false);

}  // namespace bchaves::system
