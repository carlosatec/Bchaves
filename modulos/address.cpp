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