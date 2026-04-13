/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Ponto de entrada (CLI) para o motor BSGS.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "system/cli.hpp"
#include <iostream>

int main(int argc, char** argv) {
    bchaves::system::BsgsOptions options{};
    std::string error;

    if (!bchaves::system::parse_bsgs_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n';
        std::cout << bchaves::system::bsgs_help();
        return 1;
    }

    return bchaves::engine::run_bsgs(options);
}