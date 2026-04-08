#include "engine/app.hpp"
#include "system/cli.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    qchaves::system::BsgsOptions options;
    std::string error;
    if (!qchaves::system::parse_bsgs_cli(argc, argv, options, error)) {
        std::cerr << "[E] " << error << '\n' << qchaves::system::bsgs_help();
        return 1;
    }
    if (options.help) {
        std::cout << qchaves::system::bsgs_help();
        return 0;
    }
    return qchaves::engine::run_bsgs(options);
}
