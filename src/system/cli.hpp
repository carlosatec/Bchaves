#pragma once

#include "system/types.hpp"

#include <string>

namespace qchaves::system {

bool parse_address_cli(int argc, char** argv, AddressOptions& options, std::string& error);
bool parse_bsgs_cli(int argc, char** argv, BsgsOptions& options, std::string& error);
bool parse_kangaroo_cli(int argc, char** argv, KangarooOptions& options, std::string& error);

std::string address_help();
std::string bsgs_help();
std::string kangaroo_help();

}  // namespace qchaves::system
