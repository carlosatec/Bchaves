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
#include "core/address.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"
#include "system/format.hpp"
#include "core/cuckoo.hpp"

#include <vector>
#include <array>
#include <cstdint>
#include <memory>

namespace bchaves::engine {

struct AddressMatcher {
    std::vector<std::array<std::uint8_t, 20>> hashes;
    std::unique_ptr<bchaves::core::CuckooFilter> filter;
};

int run_address(const bchaves::system::AddressOptions& options);
int run_bsgs(const bchaves::system::BsgsOptions& options);
int run_kangaroo(const bchaves::system::KangarooOptions& options);
bool configure_secp256k1_backend(bchaves::system::Secp256k1BackendPreference preference, std::string& error);

/**
 * @brief Reporta uma chave encontrada: exibe na tela e salva em found.txt.
 */
void report_found(const bchaves::core::DerivedKeyInfo& info, const std::string& context, bool persist = true);

}  // namespace bchaves::engine
