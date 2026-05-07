/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Analisador de linha de comando (CLI) e gerenciamento de opções.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "system/cli.hpp"

#include "core/hash.hpp"
#include "system/hardware.hpp"

#include <iostream>
#include <stdexcept>
#include <iomanip>

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
        options.checkpoint_enabled = false;
    } else if (arg == "--secp256k1-backend") {
        options.secp256k1_backend = parse_secp256k1_backend(require_value(argc, argv, index, arg));
    } else if (arg == "--max-ram") {
        std::string val = require_value(argc, argv, index, arg);
        if (!val.empty()) {
            char suffix = std::toupper(static_cast<unsigned char>(val.back()));
            if (suffix == 'G') {
                options.max_ram_mb = std::stoull(val.substr(0, val.size() - 1)) * 1024;
            } else if (suffix == 'M') {
                options.max_ram_mb = std::stoull(val.substr(0, val.size() - 1));
            } else {
                options.max_ram_mb = std::stoull(val);
            }
        }
    } else if (arg == "--list-hardware") {
        HardwareInfo hw = detect_hardware();
        
        std::cout << "\n[*] Hardware Report:\n";
        std::cout << "    CPU: " << hw.cpu_vendor << " " << hw.cpu_family << " " << hw.cpu_model << "\n";
        std::cout << "    Cores: " << hw.num_cores << " (Physical: " << hw.num_physical_cores;
        std::cout << ", Logical: " << hw.num_logical_cores << ", SMT: " << (hw.is_smt_enabled ? "Yes" : "No") << ")\n";
        std::cout << "    Cache: L1d=" << (hw.l1_cache / 1024) << "KB, L2=" << (hw.l2_cache / 1024) << "KB";
        std::cout << ", L3=" << (hw.l3_cache / (1024*1024)) << "MB\n";
        
        std::uint64_t ram_gb = hw.ram_total / (1024 * 1024 * 1024);
        std::cout << "    Memory: " << ram_gb << "GB DDR" << hw.memory_gen;
        std::cout << " (Channels: " << hw.memory_channels << ")\n";
        std::cout << "    Features: " << hw.isa_level;
        if (hw.features & cpu_ssse3) std::cout << ", SSSE3";
        if (hw.features & cpu_sse4) std::cout << ", SSE4";
        if (hw.features & cpu_avx) std::cout << ", AVX";
        if (hw.features & cpu_avx2) std::cout << ", AVX2";
        if (hw.features & cpu_avx512) std::cout << ", AVX512";
        if (hw.features & cpu_sha_ni) std::cout << ", SHA-NI";
        if (hw.features & cpu_bmi2) std::cout << ", BMI2";
        if (hw.features & cpu_neon) std::cout << ", NEON";
        std::cout << "\n";
        
        std::cout << "    ISA Level: " << hw.isa_level << " (" << hw.isa_level << "-bit)\n";
        std::cout << "    NUMA: " << (hw.is_numa ? "Enabled" : "Disabled") << "\n";
        
        // Show recommended tuning
        TuneProfile tune = tune_for(hw, AutoTuneProfile::balanced, 0, 0);
        std::cout << "    Recommended Tuning (balanced):\n";
        std::cout << "      Threads: " << tune.threads << ", Batch: " << tune.batch_size;
        std::cout << ", Table: " << tune.table_k << "M\n";
        
        if (hw.isa_level != "None" && hw.isa_level != "Portable") {
            std::cout << "    [Using " << hw.isa_level << " optimized SHA256";
            if (hw.isa_level == "AVX2" || hw.isa_level == "AVX512") {
                std::cout << ", " << hw.isa_level << " secp256k1";
            }
            std::cout << "]\n";
        }
        
        std::cout << "\n";
        options.help = true; // Force stop after displaying info
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
            } else if (arg == "--no-endo") {
                options.endomorphism = false;
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
        if (options.mode == SearchMode::backward || options.mode == SearchMode::both) {
            error = "Modos backward/both ainda nao estao disponiveis no address. Use sequential ou hybrid.";
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
                options.range = "bits:" + std::to_string(bits);
            } else if (arg == "--no-load") {
                options.no_load = true;
            } else if (arg == "--wild") {
                options.wild_ratio = static_cast<uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
                if (options.wild_ratio > 100) options.wild_ratio = 100;
                options.tame_ratio = 100 - options.wild_ratio;
            } else if (arg == "--tame") {
                options.tame_ratio = static_cast<uint32_t>(std::stoul(require_value(argc, argv, i, arg)));
                if (options.tame_ratio > 100) options.tame_ratio = 100;
                options.wild_ratio = 100 - options.tame_ratio;
            } else if (arg == "--trap-dir") {
                options.trap_dir = require_value(argc, argv, i, arg);
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
        if (!options.help && options.range.rfind("bits:", 0) == 0) {
            const std::uint32_t bits = static_cast<std::uint32_t>(std::stoul(options.range.substr(5)));
            if (bits == 0 || bits > 256) {
                error = "Bit range invalido (Kangaroo): use valor entre 1 e 256";
                return false;
            }
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
           "  -R <modo>      sequential|hybrid\n"
           "  -k <n>         chunk multiplier (hybrid): chunk_size = 1024 x n, min 1M\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --list-hardware exibe informacoes da CPU e encerra\n"
           "  --benchmark   sem checkpoint/found.txt\n"
           "  --secp256k1-backend <b> auto|portable\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --max-ram <n>  limite de RAM (ex: 16G, 4096M)\n"
           "  --no-checkpoint\n"
           "  --no-endo      desabilita otimizacao endomorfismo\n";
}

std::string bsgs_help() {
    return "Uso: ./bsgs <arquivo> -b <bits> [opcoes]\n"
           "  -k <n>         tamanho da tabela\n"
           "  -l <tipo>      compress|uncompress|both\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --list-hardware exibe informacoes da CPU e encerra\n"
           "  --benchmark    executa sem gravar checkpoint/found.txt\n"
           "  --secp256k1-backend <b> auto|portable\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --max-ram <n>  limite de RAM (ex: 16G, 4096M)\n"
           "  --no-checkpoint\n"
           "  --checkpoint-interval <segundos>\n";
}

std::string kangaroo_help() {
    return "Uso: ./kangaroo <arquivo|pubkey> [opcoes]\n"
           "  -b <n>         bit range (e.g., 71 para puzzle 71)\n"
           "  -r <start:end> range customizado em hexadecimal\n"
           "  -t <n>         numero de threads\n"
           "  -A <perfil>    safe|balanced|max\n"
           "  --wild <N>     % de cangurus selvagens (default: 50). Ajusta estrategia de busca.\n"
           "  --tame <N>     % de cangurus domesticados (default: 50). Ajusta estrategia de busca.\n"
           "  --no-load      pula o carregamento de armadilhas do disco (para benchmarks rapidos)\n"
           "  --list-hardware exibe informacoes da CPU e encerra\n"
           "  --benchmark    executa sem gravar checkpoint/found.txt\n"
           "  --secp256k1-backend <b> auto|portable\n"
           "  -c <arquivo>   checkpoint especifico\n"
           "  --max-ram <n>  limite de RAM (ex: 16G, 4096M)\n"
           "  --trap-dir <caminho> diretorio de armadilhas persistidas (default: traps/)\n"
           "  --no-checkpoint\n"
           "  --checkpoint-interval <segundos>\n";
}

}  // namespace bchaves::system
