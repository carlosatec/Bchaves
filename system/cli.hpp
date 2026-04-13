/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições e interfaces para o analisador CLI.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "system/types.hpp"

#include <string>

namespace bchaves::system {

bool parse_address_cli(int argc, char** argv, AddressOptions& options, std::string& error);
bool parse_bsgs_cli(int argc, char** argv, BsgsOptions& options, std::string& error);
bool parse_kangaroo_cli(int argc, char** argv, KangarooOptions& options, std::string& error);

std::string address_help();
std::string bsgs_help();
std::string kangaroo_help();

}  // namespace bchaves::system
