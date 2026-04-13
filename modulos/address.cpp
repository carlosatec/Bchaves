/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Ponto de entrada (CLI) para o motor de busca de endereços.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "system/cli.hpp"

#include <iostream>

int main(int argc, char** argv) {
    bchaves::system::AddressOptions options{};
    std::string error;

    if (!bchaves::system::parse_address_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n';
        std::cout << bchaves::system::address_help();
        return 1;
    }

    return bchaves::engine::run_address(options);
}