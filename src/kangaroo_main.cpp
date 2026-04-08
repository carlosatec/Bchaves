#include "engine/app.hpp"
#include "system/cli.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    qchaves::system::KangarooOptions options;
    std::string error;
    if (!qchaves::system::parse_kangaroo_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n' << qchaves::system::kangaroo_help();
        return 1;
    }
    if (options.help) {
        std::cout << qchaves::system::kangaroo_help();
        return 0;
    }
    return qchaves::engine::run_kangaroo(options);
}
