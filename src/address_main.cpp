#include "engine/app.hpp"
#include "system/cli.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    qchaves::system::AddressOptions options;
    std::string error;
    if (!qchaves::system::parse_address_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n' << qchaves::system::address_help();
        return 1;
    }
    if (options.help) {
        std::cout << qchaves::system::address_help();
        return 0;
    }
    return qchaves::engine::run_address(options);
}
