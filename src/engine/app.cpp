#include "engine/app.hpp"

#include "core/address.hpp"
#include "core/secp256k1.hpp"
#include "system/checkpoint.hpp"
#include "system/format.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>

namespace qchaves::engine {
namespace {

using qchaves::system::CheckpointState;
using qchaves::core::BigInt;

struct SearchRange {
    BigInt start{};
    BigInt end{};
    std::optional<std::uint32_t> puzzle_bits;
};

CheckpointState build_checkpoint(const std::string& algorithm,
                                 qchaves::system::SearchMode mode,
                                 qchaves::system::SearchType type,
                                 const qchaves::system::TuneProfile& tune) {
    CheckpointState state;
    state.algorithm = algorithm;
    state.mode = mode;
    state.type = type;
    state.threads = tune.threads;
    state.batch_size = tune.batch_size;
    state.timestamp = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return state;
}

void print_warnings(const qchaves::system::TargetLoadResult& targets) {
    for (const auto& warning : targets.warnings) {
        if (warning.line_number > 0) {
            std::cout << "[E] Linha " << warning.line_number << " ignorada: " << warning.message << '\n';
        } else {
            std::cout << "[E] " << warning.message << '\n';
        }
    }
}

void print_bootstrap(const std::string& algorithm,
                     const qchaves::system::CommonOptions& options,
                     const qchaves::system::TargetLoadResult& targets,
                     const qchaves::system::HardwareInfo& hardware,
                     const qchaves::system::TuneProfile& tune) {
    std::cout << "[+] Algorithm: " << algorithm << '\n';
    std::cout << "[+] Target source: " << options.target_path.string() << '\n';
    std::cout << "[+] Valid targets: " << targets.entries.size() << " | warnings: " << targets.warnings.size() << '\n';
    std::cout << "[+] Hardware: logical=" << hardware.num_cores << " physical=" << hardware.num_physical_cores
              << " RAM=" << qchaves::system::format_key_count(static_cast<double>(hardware.ram_available)) << "B" << '\n';
    std::cout << "[+] Auto-tune: profile=" << qchaves::system::to_string(options.auto_tune)
              << " threads=" << tune.threads << " batch=" << tune.batch_size << '\n';
}

int finalize_checkpoint(const qchaves::system::CommonOptions& options,
                        const std::filesystem::path& checkpoint_path,
                        const CheckpointState& checkpoint) {
    if (!options.checkpoint_enabled) {
        return 0;
    }
    std::string checkpoint_error;
    if (!qchaves::system::save_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
        std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
        return 1;
    }
    std::cout << "[+] Checkpoint inicial salvo em " << checkpoint_path.string() << '\n';
    return 0;
}

bool should_resume() {
    const char* env = std::getenv("QCHVES_RESUME");
    return env != nullptr && std::string(env) == "1";
}

void try_resume(const std::filesystem::path& checkpoint_path) {
    if (!std::filesystem::exists(checkpoint_path) || !should_resume()) {
        return;
    }
    qchaves::system::CheckpointState state;
    std::string error;
    if (qchaves::system::load_checkpoint(checkpoint_path, state, error)) {
        std::cout << "[+] Resume automatico: checkpoint carregado de " << checkpoint_path.string() << '\n';
    } else {
        std::cout << "[E] Checkpoint encontrado, mas invalido: " << error << '\n';
    }
}

bool to_u64_if_fits(const BigInt& value, std::uint64_t& out) {
    return qchaves::core::bigint_to_u64(value, out);
}

bool infer_puzzle_bits(const std::filesystem::path& path, std::uint32_t& bits) {
    const std::string stem = path.stem().string();
    if (stem.empty()) {
        return false;
    }
    for (const char ch : stem) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    bits = static_cast<std::uint32_t>(std::stoul(stem));
    return bits >= 1 && bits <= 256;
}

bool resolve_address_range(const qchaves::system::AddressOptions& options, SearchRange& range, std::string& error) {
    if (!options.range_start.empty() || !options.range_end.empty()) {
        if (options.range_start.empty() || options.range_end.empty()) {
            error = "range incompleto: use --start e --end juntos";
            return false;
        }
        if (!qchaves::core::parse_big_int(options.range_start, range.start) ||
            !qchaves::core::parse_big_int(options.range_end, range.end)) {
            error = "range invalido: use valores hex ou decimal";
            return false;
        }
    } else {
        std::uint32_t bits = 0;
        if (!infer_puzzle_bits(options.target_path, bits)) {
            error = "nao foi possivel inferir range do puzzle; use --start e --end";
            return false;
        }
        range.puzzle_bits = bits;
        if (bits == 1) {
            range.start = 1;
            range.end = 1;
        } else {
            range.start = BigInt(1) << (bits - 1u);
            range.end = (BigInt(1) << bits) - 1;
        }
    }

    if (range.start <= 0 || range.end < range.start) {
        error = "range invalido: inicio/fim fora do dominio";
        return false;
    }
    const BigInt max_key = qchaves::core::secp256k1_curve_order() - 1;
    if (range.start > max_key) {
        error = "range invalido: inicio excede a ordem da curva secp256k1";
        return false;
    }
    if (range.end > max_key) {
        range.end = max_key;
    }
    return true;
}

bool type_allows_compressed(qchaves::system::SearchType type) {
    return type == qchaves::system::SearchType::compress || type == qchaves::system::SearchType::both;
}

bool type_allows_uncompressed(qchaves::system::SearchType type) {
    return type == qchaves::system::SearchType::uncompress || type == qchaves::system::SearchType::both;
}

bool target_matches(const qchaves::system::TargetEntry& target,
                    const qchaves::system::SearchType type,
                    const qchaves::core::DerivedKeyInfo& key_info) {
    if (target.type == qchaves::system::TargetType::address_btc) {
        return (type_allows_compressed(type) && target.payload == key_info.address_payload_compressed) ||
               (type_allows_uncompressed(type) && target.payload == key_info.address_payload_uncompressed);
    }
    if (target.type == qchaves::system::TargetType::pubkey_compress) {
        return type_allows_compressed(type) && target.payload == key_info.pubkey_compressed;
    }
    if (target.type == qchaves::system::TargetType::pubkey_uncompress) {
        return type_allows_uncompressed(type) && target.payload == key_info.pubkey_uncompressed;
    }
    return false;
}

std::optional<std::uint32_t> detect_puzzle_id(const std::filesystem::path& target_source) {
    const std::string stem = target_source.stem().string();
    if (stem.empty()) {
        return std::nullopt;
    }
    for (const char ch : stem) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
    }
    return static_cast<std::uint32_t>(std::stoul(stem));
}

std::string current_timestamp_local() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_now);
#else
    localtime_r(&time_now, &tm_now);
#endif
    std::ostringstream out;
    out << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string current_timestamp_iso8601_local() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_now);
#else
    localtime_r(&time_now, &tm_now);
#endif
    std::ostringstream out;
    out << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

void checkpoint_bigint(const BigInt& value, std::array<std::uint8_t, 32>& out) {
    BigInt temp = value;
    for (int i = 31; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(temp.to_u64() & 0xffu);
        temp = temp >> 8u;
    }
}

bool persist_checkpoint(const std::filesystem::path& checkpoint_path,
                        const CheckpointState& checkpoint,
                        std::string& error) {
    return qchaves::system::save_checkpoint(checkpoint_path, checkpoint, error);
}

void append_found_record(const std::filesystem::path& target_source,
                         const qchaves::system::SearchType type,
                         const qchaves::core::DerivedKeyInfo& key_info,
                         std::uint32_t thread_id,
                         const std::chrono::steady_clock::duration& elapsed) {
    std::ofstream out("FOUND.txt", std::ios::app);
    if (!out) {
        return;
    }
    const auto puzzle_id = detect_puzzle_id(target_source);
    const std::string timestamp_display = current_timestamp_local();
    const std::string timestamp_iso = current_timestamp_iso8601_local();
    const auto seconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
    out << "# Qchaves v2.0 - Chave Encontrada\n";
    if (puzzle_id.has_value()) {
        out << "# Puzzle: " << puzzle_id.value() << '\n';
    }
    out << "# Data: " << timestamp_display << "\n\n";
    if (puzzle_id.has_value()) {
        out << "PuzzleID=" << puzzle_id.value() << '\n';
    }
    out << "TargetSource=" << target_source.string() << '\n';
    out << "PrivateKeyHex=" << qchaves::core::bigint_to_hex(key_info.private_key) << '\n';
    out << "PrivateKeyDec=" << qchaves::core::bigint_to_decimal(key_info.private_key) << '\n';
    out << "WIFCompressed=" << key_info.wif_compressed << '\n';
    out << "WIFUncompressed=" << key_info.wif_uncompressed << '\n';
    out << "PublicKeyCompressed=" << key_info.pubkey_compressed_hex << '\n';
    out << "PublicKeyUncompressed=" << key_info.pubkey_uncompressed_hex << '\n';
    out << "AddressCompressed=" << key_info.address_compressed << '\n';
    out << "AddressUncompressed=" << key_info.address_uncompressed << '\n';
    out << "Mode=address\n";
    out << "Type=" << qchaves::system::to_string(type) << '\n';
    out << "Thread=" << thread_id << '\n';
    out << "Timestamp=" << timestamp_iso << '\n';
    out << "Elapsed=" << qchaves::system::format_duration(seconds) << "\n\n";
}

void print_found(const std::filesystem::path& target_source,
                 const qchaves::system::SearchType type,
                 const qchaves::core::DerivedKeyInfo& key_info,
                 std::uint32_t thread_id,
                 const std::chrono::steady_clock::duration& elapsed) {
    const auto puzzle_id = detect_puzzle_id(target_source);
    const auto seconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
    if (puzzle_id.has_value()) {
        std::cout << "[!] FOUND! [Puzzle #" << puzzle_id.value() << "]\n";
    } else {
        std::cout << "[!] FOUND!\n";
    }
    std::cout << "    Target: " << target_source.string() << '\n';
    std::cout << "    Private Key Hex: " << qchaves::core::bigint_to_hex(key_info.private_key) << '\n';
    std::cout << "    Private Key Dec: " << qchaves::core::bigint_to_decimal(key_info.private_key) << '\n';
    std::cout << "    WIF Compressed: " << key_info.wif_compressed << '\n';
    std::cout << "    WIF Uncompressed: " << key_info.wif_uncompressed << '\n';
    std::cout << "    Public Key Compressed: " << key_info.pubkey_compressed_hex << '\n';
    std::cout << "    Public Key Uncompressed: " << key_info.pubkey_uncompressed_hex << '\n';
    std::cout << "    Address Compressed: " << key_info.address_compressed << '\n';
    std::cout << "    Address Uncompressed: " << key_info.address_uncompressed << '\n';
    std::cout << "    Mode: Address | " << qchaves::system::to_string(type) << '\n';
    std::cout << "    Thread: " << thread_id << '\n';
    std::cout << "    Time: " << qchaves::system::format_duration(seconds) << '\n';
}

void print_address_stats(qchaves::system::SearchMode mode,
                         std::uint64_t processed,
                         std::uint64_t processed_forward,
                         std::uint64_t processed_backward,
                         const std::chrono::steady_clock::duration& elapsed,
                         const SearchRange& range) {
    const auto seconds = std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()));
    const double rate = static_cast<double>(processed) / static_cast<double>(seconds);
    if (mode == qchaves::system::SearchMode::random) {
        std::cout << "[Random] Keys: " << qchaves::system::format_key_count(static_cast<double>(processed))
                  << " | Rate: " << qchaves::system::format_rate(rate)
                  << " | Time: " << qchaves::system::format_duration(seconds) << '\n';
        return;
    }

    std::uint64_t width = 0;
    if (to_u64_if_fits(range.end - range.start + 1, width) && width > 0) {
        const double ratio = std::min(1.0, static_cast<double>(processed) / static_cast<double>(width));
        std::string eta_text;
        if (processed > 0 && ratio < 1.0 && rate > 0.0) {
            const double remaining = static_cast<double>(width - processed) / rate;
            eta_text = " | ETA: " + qchaves::system::format_duration(static_cast<std::uint64_t>(remaining));
        }
        std::cout << qchaves::system::make_progress_bar(ratio)
                  << ' ' << static_cast<int>(ratio * 100.0) << "% | Total Keys: "
                  << qchaves::system::format_key_count(static_cast<double>(processed))
                  << " | Rate: " << qchaves::system::format_rate(rate)
                  << eta_text << '\n';
        if (mode == qchaves::system::SearchMode::both) {
            const std::uint64_t forward_total = (width + 1u) / 2u;
            const std::uint64_t backward_total = width / 2u;
            const double forward_ratio = forward_total == 0 ? 1.0 : std::min(1.0, static_cast<double>(processed_forward) / static_cast<double>(forward_total));
            const double backward_ratio = backward_total == 0 ? 1.0 : std::min(1.0, static_cast<double>(processed_backward) / static_cast<double>(backward_total));
            std::cout << "Fwd: " << qchaves::system::make_progress_bar(forward_ratio)
                      << ' ' << static_cast<int>(forward_ratio * 100.0) << "%\n";
            std::cout << "Bwd: " << qchaves::system::make_progress_bar(backward_ratio)
                      << ' ' << static_cast<int>(backward_ratio * 100.0) << "%\n";
        }
    } else {
        std::cout << "[+] Keys: " << qchaves::system::format_key_count(static_cast<double>(processed))
                  << " | Rate: " << qchaves::system::format_rate(rate)
                  << " | Time: " << qchaves::system::format_duration(seconds) << '\n';
    }
}

}  // namespace

int run_address(const qchaves::system::AddressOptions& options) {
    const auto targets = qchaves::system::load_targets(options.target_path, false);
    print_warnings(targets);
    if (targets.entries.empty()) {
        std::cerr << "[E] Nenhum alvo valido carregado" << '\n';
        return 1;
    }

    const auto hardware = qchaves::system::detect_hardware();
    const auto tune = qchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    print_bootstrap("address", options, targets, hardware, tune);

    SearchRange range;
    std::string range_error;
    if (!resolve_address_range(options, range, range_error)) {
        std::cerr << "[E] " << range_error << '\n';
        return 1;
    }
    std::cout << "[+] Range start: " << qchaves::core::bigint_to_hex(range.start) << '\n';
    std::cout << "[+] Range end:   " << qchaves::core::bigint_to_hex(range.end) << '\n';
    if (range.puzzle_bits.has_value()) {
        std::cout << "[+] Puzzle bits inferidos: " << range.puzzle_bits.value() << '\n';
    }
    if (tune.threads > 1) {
        std::cout << "[i] Backend portatil de referencia usando 1 thread efetiva nesta fase." << '\n';
    }

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("address", range.puzzle_bits));
    try_resume(checkpoint_path);
    auto checkpoint = build_checkpoint("address", options.mode, options.type, tune);
    checkpoint_bigint(range.start, checkpoint.range_start);
    checkpoint_bigint(range.end, checkpoint.range_end);
    checkpoint_bigint(range.start, checkpoint.current);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }

    std::mt19937_64 rng(std::random_device{}());
    BigInt forward = range.start;
    BigInt backward = range.end;
    BigInt last_processed = range.start;
    bool use_forward = true;
    std::uint64_t processed = 0;
    std::uint64_t processed_forward = 0;
    std::uint64_t processed_backward = 0;
    const auto started = std::chrono::steady_clock::now();
    auto last_stats = started;
    auto last_checkpoint = started;

    while (true) {
        if (options.limit > 0 && processed >= options.limit) {
            break;
        }

        BigInt current;
        bool current_from_forward = true;
        if (options.mode == qchaves::system::SearchMode::sequential) {
            if (forward > range.end) break;
            current = forward;
            ++forward;
        } else if (options.mode == qchaves::system::SearchMode::backward) {
            if (backward < range.start) break;
            current = backward;
            --backward;
            current_from_forward = false;
        } else if (options.mode == qchaves::system::SearchMode::both) {
            if (forward > backward) break;
            current_from_forward = use_forward;
            if (current_from_forward) {
                current = forward;
                ++forward;
            } else {
                current = backward;
                --backward;
            }
            use_forward = !use_forward;
        } else {
            const BigInt width = range.end - range.start + 1;
            std::uint64_t width_u64 = 0;
            if (!to_u64_if_fits(width, width_u64) || width_u64 == 0) {
                std::cerr << "[E] Random mode do backend portatil requer range <= 64 bits" << '\n';
                return 1;
            }
            current = range.start + std::uniform_int_distribution<std::uint64_t>(0, width_u64 - 1)(rng);
        }

        qchaves::core::DerivedKeyInfo key_info;
        if (!qchaves::core::derive_key_info(current, key_info)) {
            std::cerr << "[E] Chave privada invalida no range atual" << '\n';
            return 1;
        }

        last_processed = current;
        ++processed;
        if (options.mode == qchaves::system::SearchMode::random) {
            ++processed_forward;
            processed_backward = 0;
        } else if (current_from_forward) {
            ++processed_forward;
        } else {
            ++processed_backward;
        }
        for (const auto& target : targets.entries) {
            if (!target_matches(target, options.type, key_info)) {
                continue;
            }
            const auto elapsed = std::chrono::steady_clock::now() - started;
            checkpoint_bigint(last_processed, checkpoint.current);
            std::string checkpoint_error;
            if (options.checkpoint_enabled &&
                !persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            print_found(options.target_path, options.type, key_info, 1u, elapsed);
            append_found_record(options.target_path, options.type, key_info, 1u, elapsed);
            return 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (options.checkpoint_enabled &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            checkpoint_bigint(last_processed, checkpoint.current);
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            last_checkpoint = now;
        }
        if (now - last_stats >= std::chrono::seconds(30)) {
            print_address_stats(options.mode, processed, processed_forward, processed_backward, now - started, range);
            last_stats = now;
        }
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    checkpoint_bigint(last_processed, checkpoint.current);
    if (options.checkpoint_enabled) {
        std::string checkpoint_error;
        if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
            std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
            return 1;
        }
    }
    print_address_stats(options.mode, processed, processed_forward, processed_backward, elapsed, range);
    std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
    return 0;
}

int run_bsgs(const qchaves::system::BsgsOptions& options) {
    const auto targets = qchaves::system::load_targets(options.target_path, true);
    print_warnings(targets);
    if (targets.entries.empty()) {
        std::cerr << "[E] Nenhuma public key valida carregada" << '\n';
        return 1;
    }

    const auto hardware = qchaves::system::detect_hardware();
    const auto tune = qchaves::system::tune_for(hardware, options.auto_tune, options.threads, options.table_k);
    print_bootstrap("bsgs", options, targets, hardware, tune);
    std::cout << "[+] Table preset (-k): " << tune.table_k << " | bit range: " << options.bits << '\n';

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("bsgs", options.bits));
    try_resume(checkpoint_path);
    const auto checkpoint = build_checkpoint("bsgs", qchaves::system::SearchMode::sequential, options.type, tune);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }

    std::cout << "[+] Base BSGS pronta para receber tabela, filtros e giant steps reais" << '\n';
    return 0;
}

int run_kangaroo(const qchaves::system::KangarooOptions& options) {
    const auto targets = qchaves::system::load_targets(options.target_path, true);
    print_warnings(targets);
    if (targets.entries.empty()) {
        std::cerr << "[E] Nenhuma public key valida carregada" << '\n';
        return 1;
    }

    const auto hardware = qchaves::system::detect_hardware();
    const auto tune = qchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    print_bootstrap("kangaroo", options, targets, hardware, tune);
    std::cout << "[+] Range: " << options.range << '\n';

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("kangaroo"));
    try_resume(checkpoint_path);
    const auto checkpoint = build_checkpoint("kangaroo", qchaves::system::SearchMode::random, qchaves::system::SearchType::both, tune);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }

    std::cout << "[+] Base Kangaroo pronta para integrar jumps, traps e distinguished points" << '\n';
    return 0;
}

}  // namespace qchaves::engine
