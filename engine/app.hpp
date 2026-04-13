/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições globais e cabeçalho base para motores de busca.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "system/types.hpp"

#include "system/types.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"
#include "system/format.hpp"

#include <vector>
#include <array>
#include <cstdint>

namespace bchaves::engine {

struct AddressMatcher {
    std::vector<std::array<std::uint8_t, 20>> hashes;
};

int run_address(const bchaves::system::AddressOptions& options);
int run_bsgs(const bchaves::system::BsgsOptions& options);
int run_kangaroo(const bchaves::system::KangarooOptions& options);

}  // namespace bchaves::engine
