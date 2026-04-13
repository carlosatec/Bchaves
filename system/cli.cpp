#include "system/cli.hpp"

#include "core/hash.hpp"

#include <iostream>
#include <stdexcept>

namespace bchaves::system {
namespace {

AutoTuneProfile parse_profile(const std::string& value) {
    const std::string lower = bchaves::core::to_lower(value);
    if (lower == "safe") return AutoTuneProfile::safe;
    if (lower == "balanced") return AutoTuneProfile::balanced;
    if (lower == "max") return AutoTuneProfile::max;
    throw std::runtime_error("Auto-tune invalido: " + value);
}

Secp256k1BackendPreference parse_secp256k1_backend(const std::string& value) {
    const std::string lower = bchaves::core::to_lower(value);
    if (lower == "auto") return Secp256k1BackendPreference::auto_select;
    if (lower == "portable") return Secp256k1BackendPreference::portable;
    if (lower == "external") return Secp256k1BackendPreference::external;
    throw std::runtime_error("Backend secp256k1 invalido: " + value);
}

SearchMode parse_mode(const std::string& value) {
    const std::string lower = bchaves::core::to_lower(value);
    if (lower == "sequential") return SearchMode::sequential;
    if (lower == "backward") return SearchMode::backward;
    if (lower == "both") return SearchMode::both;
    if (lower == "hybrid") return SearchMode::hybrid;
    throw std::runtime_error("Modo de busca invalido: " + value);
}

SearchType parse_type(const std::string& value) {
    const std::string lower = bchaves::core::to_lower(value);
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
    } else if (arg == "--benchmark") {
        options.benchmark = true;
    } else if (arg == "--secp256k1-backend") {
        options.secp256k1_backend = parse_secp256k1_backend(require_value(argc, argv, index, arg));
    } else if (arg == "--list-hardware") {
        std::cout << "[*] Comando list-hardware detectado...\n";
        options.help = true; // Força parada após exibir info
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
            } else if (arg == "-b") {
                options.bits = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
            } else if (arg == "-k") {
                options.chunk_k = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
            } else if (!arg.empty() && arg[0] != '-') {
                options.target_path = arg;
            } else {
                throw std::runtime_error("Opcao invalida: " + arg);
            }
        }
        if (options.bits == 0 || options.bits > 256) {
            error = "Use -b <bits> (1-256)";
            return false;
        }
        if (options.mode == SearchMode::hybrid && options.chunk_k == 0) {
            error = "Modo hybrid requer -k <multiplicador> (ex: -k 1024 = 1M chaves/chunk)";
            return false;
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
            } else if (arg == "-b") {
                uint32_t bits = static_cast<std::uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
                // Converte bits para range hexadecimal (ex: 71 -> 4000...:7fff...)
                // Isso será tratado no engine/kangaroo.cpp se options.range estiver vazio mas bits > 0
                options.range = "bits:" + std::to_string(bits);
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
    return "Uso: ./address <arquivo> -b <bits> [opcoes]\n"
           "  -b <n>         bit range (e.g., 71 for puzzle 71)\n"
           "  -R <modo>      sequential|backward|both|hybrid\n"
           "  -k <n>         chunk multiplier (hybrid): chunk_size = 1024 x n, min 1M\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --benchmark   sem checkpoint/FOUND.txt\n"
           "  --secp256k1-backend <b> auto|portable|external\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --no-checkpoint\n";
}

std::string bsgs_help() {
    return "Uso: ./bsgs <arquivo> -b <bits> [opcoes]\n"
           "  -k <n>         tamanho da tabela\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --benchmark    executa sem gravar checkpoint/FOUND.txt\n"
           "  --secp256k1-backend <b> auto|portable|external\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --no-checkpoint\n"
           "  --checkpoint-interval <segundos>\n";
}

std::string kangaroo_help() {
    return "Uso: ./kangaroo <arquivo|pubkey> [opcoes]\n"
           "  -b <n>         bit range (e.g., 71 para puzzle 71)\n"
           "  -r <start:end> range customizado em hexadecimal\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --list-hardware exibe informações da CPU e encerra\n"
           "  --benchmark    executa sem gravar checkpoint/FOUND.txt\n"
           "  --secp256k1-backend <b> auto|portable|external\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --no-checkpoint\n"
           "  --checkpoint-interval <segundos>\n";
}

}  // namespace bchaves::system
