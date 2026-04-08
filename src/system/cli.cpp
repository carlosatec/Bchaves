#include "system/cli.hpp"

#include "core/hash.hpp"

#include <stdexcept>

namespace qchaves::system {
namespace {

AutoTuneProfile parse_profile(const std::string& value) {
    const std::string lower = qchaves::core::to_lower(value);
    if (lower == "safe") return AutoTuneProfile::safe;
    if (lower == "balanced") return AutoTuneProfile::balanced;
    if (lower == "max") return AutoTuneProfile::max;
    throw std::runtime_error("Auto-tune invalido: " + value);
}

SearchMode parse_mode(const std::string& value) {
    const std::string lower = qchaves::core::to_lower(value);
    if (lower == "sequential") return SearchMode::sequential;
    if (lower == "backward") return SearchMode::backward;
    if (lower == "both") return SearchMode::both;
    if (lower == "random") return SearchMode::random;
    throw std::runtime_error("Modo de busca invalido: " + value);
}

SearchType parse_type(const std::string& value) {
    const std::string lower = qchaves::core::to_lower(value);
    if (lower == "compress") return SearchType::compress;
    if (lower == "uncompress") return SearchType::uncompress;
    if (lower == "both") return SearchType::both;
    throw std::runtime_error("Tipo de busca invalido: " + value);
}

std::string require_value(int argc, char** argv, int& index, const std::string& option) {
    if (index + 1 >= argc) {
        throw std::runtime_error("Opcao requer valor: " + option);
    }
    ++index;
    return argv[index];
}

void parse_common_flag(const std::string& arg, int argc, char** argv, int& index, CommonOptions& options, bool& handled) {
    handled = true;
    if (arg == "-t") {
        options.threads = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, index, arg)));
    } else if (arg == "-A" || arg == "--auto-tune") {
        options.auto_tune = parse_profile(require_value(argc, argv, index, arg));
    } else if (arg == "-c" || arg == "--checkpoint") {
        options.checkpoint_path = require_value(argc, argv, index, arg);
    } else if (arg == "--no-checkpoint") {
        options.checkpoint_enabled = false;
    } else if (arg == "--checkpoint-interval") {
        options.checkpoint_interval_seconds = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, index, arg)));
    } else if (arg == "-h" || arg == "--help") {
        options.help = true;
    } else {
        handled = false;
    }
}

bool ensure_target(const CommonOptions& options, std::string& error) {
    if (options.help) {
        return true;
    }
    if (options.target_path.empty()) {
        error = "Arquivo de alvo nao especificado";
        return false;
    }
    return true;
}

}  // namespace

bool parse_address_cli(int argc, char** argv, AddressOptions& options, std::string& error) {
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            bool handled = false;
            parse_common_flag(arg, argc, argv, i, options, handled);
            if (handled) continue;
            if (arg == "-R") {
                options.mode = parse_mode(require_value(argc, argv, i, arg));
            } else if (arg == "-l") {
                options.type = parse_type(require_value(argc, argv, i, arg));
            } else if (arg == "--start") {
                options.range_start = require_value(argc, argv, i, arg);
            } else if (arg == "--end") {
                options.range_end = require_value(argc, argv, i, arg);
            } else if (arg == "--limit") {
                options.limit = static_cast<std::uint64_t>(std::stoull(require_value(argc, argv, i, arg)));
            } else if (!arg.empty() && arg[0] != '-') {
                options.target_path = arg;
            } else {
                throw std::runtime_error("Opcao invalida: " + arg);
            }
        }
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    return ensure_target(options, error);
}

bool parse_bsgs_cli(int argc, char** argv, BsgsOptions& options, std::string& error) {
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            bool handled = false;
            parse_common_flag(arg, argc, argv, i, options, handled);
            if (handled) continue;
            if (arg == "-b") {
                options.bits = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
            } else if (arg == "-k") {
                options.table_k = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
            } else if (arg == "-l") {
                options.type = parse_type(require_value(argc, argv, i, arg));
            } else if (!arg.empty() && arg[0] != '-') {
                options.target_path = arg;
            } else {
                throw std::runtime_error("Opcao invalida: " + arg);
            }
        }
        if (!options.help && (options.bits == 0 || options.bits > 256)) {
            error = "Bit range invalido: use valor entre 1 e 256";
            return false;
        }
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    return ensure_target(options, error);
}

bool parse_kangaroo_cli(int argc, char** argv, KangarooOptions& options, std::string& error) {
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            bool handled = false;
            parse_common_flag(arg, argc, argv, i, options, handled);
            if (handled) continue;
            if (arg == "-r") {
                options.range = require_value(argc, argv, i, arg);
            } else if (!arg.empty() && arg[0] != '-') {
                options.target_path = arg;
            } else {
                throw std::runtime_error("Opcao invalida: " + arg);
            }
        }
        if (!options.help && options.range.empty()) {
            error = "Range invalido (Kangaroo): use -r <start:end>";
            return false;
        }
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    return ensure_target(options, error);
}

std::string address_help() {
    return "Uso: ./address <arquivo> [opcoes]\n"
           "  -R <modo>      sequential|backward|both|random\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  --start <n>    inicio do range (hex ou decimal)\n"
           "  --end <n>      fim do range (hex ou decimal)\n"
           "  --limit <n>    limite de chaves para processar\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --no-checkpoint\n"
           "  --checkpoint-interval <segundos>\n";
}

std::string bsgs_help() {
    return "Uso: ./bsgs <arquivo> -b <bits> [opcoes]\n"
           "  -k <n>         tamanho da tabela\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n";
}

std::string kangaroo_help() {
    return "Uso: ./kangaroo <arquivo|pubkey> -r <start:end> [opcoes]\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n";
}

}  // namespace qchaves::system
