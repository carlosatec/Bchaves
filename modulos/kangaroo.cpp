#include "engine/app.hpp"
#include "system/cli.hpp"
#include <iostream>

int main(int argc, char** argv) {
    bchaves::system::KangarooOptions options{};
    std::string error;

    if (!bchaves::system::parse_kangaroo_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n';
        std::cout << bchaves::system::kangaroo_help();
        return 1;
    }

    return bchaves::engine::run_kangaroo(options);
}