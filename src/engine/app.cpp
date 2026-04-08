#include "engine/app.hpp"

#include "core/address.hpp"
#include "core/secp256k1.hpp"
#include "system/checkpoint.hpp"
#include "system/format.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace qchaves::engine {
namespace {

using qchaves::system::CheckpointState;
using qchaves::core::BigInt;

volatile std::sig_atomic_t g_interrupt_requested = 0;

struct SearchRange {
    BigInt start{};
    BigInt end{};
    std::optional<std::uint32_t> puzzle_bits;
};

struct ResumeState {
    bool active = false;
    CheckpointState checkpoint{};
};

struct AddressHit {
    qchaves::core::DerivedKeyInfo key_info{};
    std::uint32_t thread_id = 0;
};

struct AddressParallelState {
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> processed_total{0};
    std::atomic<std::uint64_t> processed_forward{0};
    std::atomic<std::uint64_t> processed_backward{0};
    std::atomic<std::uint64_t> active_workers{0};
    std::atomic<std::uint64_t> next_forward_offset{0};
    std::atomic<std::uint64_t> next_backward_offset{0};
    std::atomic<std::uint64_t> random_checkpoint{0};
    std::mutex hit_mutex;
    bool hit_found = false;
    AddressHit hit{};
};

void checkpoint_bigint(const BigInt& value, std::array<std::uint8_t, 32>& out);
BigInt checkpoint_to_bigint(const std::array<std::uint8_t, 32>& bytes);

void handle_interrupt_signal(int) {
    g_interrupt_requested = 1;
}

class ScopedInterruptHandler {
public:
    ScopedInterruptHandler() {
        g_interrupt_requested = 0;
        previous_sigint_ = std::signal(SIGINT, handle_interrupt_signal);
    }

    ~ScopedInterruptHandler() {
        std::signal(SIGINT, previous_sigint_);
        g_interrupt_requested = 0;
    }

private:
    using SignalHandler = void (*)(int);
    SignalHandler previous_sigint_ = SIG_DFL;
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
    const auto& backend = qchaves::core::secp256k1_backend_info();
    std::cout << "[+] secp256k1 backend: " << backend.name
              << " | optimized=" << (backend.optimized ? "yes" : "no")
              << " | external=" << (backend.external ? "yes" : "no") << '\n';
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

ResumeState load_resume_state(const std::filesystem::path& checkpoint_path) {
    ResumeState result;
    if (!std::filesystem::exists(checkpoint_path) || !should_resume()) {
        return result;
    }

    std::string error;
    if (!qchaves::system::load_checkpoint(checkpoint_path, result.checkpoint, error)) {
        std::cout << "[E] Checkpoint encontrado, mas invalido: " << error << '\n';
        return result;
    }

    result.active = true;
    std::cout << "[+] Resume automatico: checkpoint carregado de " << checkpoint_path.string() << '\n';
    return result;
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

bool resolve_bsgs_range(const qchaves::system::BsgsOptions& options, SearchRange& range, std::string& error) {
    if (options.bits < 1 || options.bits > 256) {
        error = "bit range invalido";
        return false;
    }

    range.puzzle_bits = options.bits;
    if (options.bits == 1) {
        range.start = 1;
        range.end = 1;
    } else {
        range.start = BigInt(1) << (options.bits - 1u);
        range.end = (BigInt(1) << options.bits) - 1;
    }

    const BigInt max_key = qchaves::core::secp256k1_curve_order() - 1;
    if (range.end > max_key) {
        range.end = max_key;
    }
    return true;
}

bool parse_range_expression(const std::string& text, SearchRange& range, std::string& error) {
    const std::size_t separator = text.find(':');
    if (separator == std::string::npos) {
        error = "range invalido: use <start:end>";
        return false;
    }

    const std::string start_text = text.substr(0, separator);
    const std::string end_text = text.substr(separator + 1);
    if (start_text.empty() || end_text.empty()) {
        error = "range invalido: use <start:end>";
        return false;
    }
    if (!qchaves::core::parse_big_int(start_text, range.start) ||
        !qchaves::core::parse_big_int(end_text, range.end)) {
        error = "range invalido: use valores hex ou decimal";
        return false;
    }
    if (range.end < range.start) {
        error = "range invalido: fim menor que inicio";
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

bool apply_resume_for_range(const std::string& algorithm,
                            qchaves::system::SearchMode mode,
                            qchaves::system::SearchType type,
                            const ResumeState& resume,
                            const SearchRange& original_range,
                            SearchRange& active_range,
                            CheckpointState& checkpoint,
                            bool& resume_completed) {
    if (!resume.active || resume.checkpoint.algorithm != algorithm) {
        return false;
    }

    const BigInt resume_start = checkpoint_to_bigint(resume.checkpoint.range_start);
    const BigInt resume_end = checkpoint_to_bigint(resume.checkpoint.range_end);
    const BigInt resume_current = checkpoint_to_bigint(resume.checkpoint.current);
    if (resume.checkpoint.mode != mode ||
        resume.checkpoint.type != type ||
        resume_start != original_range.start ||
        resume_end != original_range.end) {
        std::cout << "[i] Checkpoint encontrado, mas nao compativel com a execucao atual" << '\n';
        return false;
    }

    checkpoint = resume.checkpoint;

    if (mode == qchaves::system::SearchMode::sequential) {
        if (resume_current < original_range.end) {
            active_range.start = resume_current + BigInt(1);
            checkpoint_bigint(active_range.start, checkpoint.current);
            std::cout << "[+] Resume aplicado: proxima chave "
                      << qchaves::core::bigint_to_hex(active_range.start) << '\n';
            return true;
        }
        std::cout << "[+] Resume indica range ja concluido" << '\n';
        resume_completed = true;
        return true;
    }

    if (mode == qchaves::system::SearchMode::backward) {
        if (resume_current > original_range.start) {
            active_range.end = resume_current - BigInt(1);
            checkpoint_bigint(active_range.end, checkpoint.current);
            std::cout << "[+] Resume aplicado: proxima chave "
                      << qchaves::core::bigint_to_hex(active_range.end) << '\n';
            return true;
        }
        std::cout << "[+] Resume indica range ja concluido" << '\n';
        resume_completed = true;
        return true;
    }

    std::cout << "[i] Resume automatico ainda nao e aplicado para modo "
              << qchaves::system::to_string(mode) << '\n';
    return true;
}

std::uint64_t estimate_processed_offset(qchaves::system::SearchMode mode,
                                        const SearchRange& original_range,
                                        const SearchRange& active_range) {
    std::uint64_t processed = 0;
    if (mode == qchaves::system::SearchMode::sequential &&
        to_u64_if_fits(active_range.start - original_range.start, processed)) {
        return processed;
    }
    if (mode == qchaves::system::SearchMode::backward &&
        to_u64_if_fits(original_range.end - active_range.end, processed)) {
        return processed;
    }
    return 0;
}

bool can_parallelize_address_range(const SearchRange& range,
                                   std::uint32_t threads,
                                   std::uint64_t& width_u64) {
    if (threads <= 1) {
        return false;
    }
    if (!to_u64_if_fits(range.end - range.start + BigInt(1), width_u64) || width_u64 == 0) {
        return false;
    }
    return true;
}

BigInt compute_parallel_checkpoint_current(const qchaves::system::AddressOptions& options,
                                           const SearchRange& range,
                                           const qchaves::system::TuneProfile& tune,
                                           const AddressParallelState& state,
                                           std::uint64_t width_u64) {
    const std::uint64_t safety_window = std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(tune.batch_size)) *
                                        std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(tune.threads));
    if (options.mode == qchaves::system::SearchMode::sequential) {
        const std::uint64_t assigned = std::min(width_u64, state.next_forward_offset.load(std::memory_order_relaxed));
        const std::uint64_t safe_offset = assigned > safety_window ? assigned - safety_window : 0;
        return range.start + BigInt(safe_offset);
    }
    if (options.mode == qchaves::system::SearchMode::backward) {
        const std::uint64_t assigned = std::min(width_u64, state.next_backward_offset.load(std::memory_order_relaxed));
        const std::uint64_t safe_offset = assigned > safety_window ? assigned - safety_window : 0;
        return range.end - BigInt(safe_offset);
    }
    if (options.mode == qchaves::system::SearchMode::random) {
        return range.start + BigInt(state.random_checkpoint.load(std::memory_order_relaxed));
    }
    return range.start;
}

void store_parallel_hit(AddressParallelState& state,
                        std::uint32_t thread_id,
                        const qchaves::core::DerivedKeyInfo& key_info) {
    std::lock_guard<std::mutex> lock(state.hit_mutex);
    if (state.hit_found) {
        return;
    }
    state.hit_found = true;
    state.hit.thread_id = thread_id;
    state.hit.key_info = key_info;
    state.stop.store(true, std::memory_order_relaxed);
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

BigInt checkpoint_to_bigint(const std::array<std::uint8_t, 32>& bytes) {
    BigInt value;
    for (const std::uint8_t byte : bytes) {
        value = value << 8u;
        value += BigInt(byte);
    }
    return value;
}

bool persist_checkpoint(const std::filesystem::path& checkpoint_path,
                        const CheckpointState& checkpoint,
                        std::string& error) {
    return qchaves::system::save_checkpoint(checkpoint_path, checkpoint, error);
}

bool interrupt_requested() {
    return g_interrupt_requested != 0;
}

void update_checkpoint_current(CheckpointState& checkpoint, const BigInt& current) {
    checkpoint.timestamp = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    checkpoint_bigint(current, checkpoint.current);
}

int finalize_interrupted_run(const qchaves::system::CommonOptions& options,
                             const std::filesystem::path& checkpoint_path,
                             CheckpointState& checkpoint,
                             const BigInt& last_processed) {
    std::cout << "[!] Interrupcao detectada (Ctrl+C)" << '\n';
    if (!options.checkpoint_enabled) {
        std::cout << "[i] Checkpoint desabilitado; estado atual nao sera salvo" << '\n';
        return 130;
    }

    update_checkpoint_current(checkpoint, last_processed);
    std::string checkpoint_error;
    if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
        std::cerr << "[E] Falha ao salvar checkpoint de emergencia: " << checkpoint_error << '\n';
        return 1;
    }

    std::cout << "[+] Checkpoint de emergencia salvo em " << checkpoint_path.string() << '\n';
    return 130;
}

void append_found_record(const std::string& algorithm,
                         const std::filesystem::path& target_source,
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
    out << "Mode=" << algorithm << '\n';
    out << "Type=" << qchaves::system::to_string(type) << '\n';
    out << "Thread=" << thread_id << '\n';
    out << "Timestamp=" << timestamp_iso << '\n';
    out << "Elapsed=" << qchaves::system::format_duration(seconds) << "\n\n";
}

void print_found(const std::string& algorithm,
                 const std::filesystem::path& target_source,
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
    std::cout << "    Mode: " << algorithm << " | " << qchaves::system::to_string(type) << '\n';
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

void print_bsgs_stats(std::uint64_t processed,
                      const std::chrono::steady_clock::duration& elapsed,
                      const SearchRange& range) {
    const auto seconds = std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()));
    const double rate = static_cast<double>(processed) / static_cast<double>(seconds);
    std::uint64_t width = 0;
    if (to_u64_if_fits(range.end - range.start + 1, width) && width > 0) {
        const double ratio = std::min(1.0, static_cast<double>(processed) / static_cast<double>(width));
        std::string eta_text;
        if (processed > 0 && ratio < 1.0 && rate > 0.0) {
            const double remaining = static_cast<double>(width - processed) / rate;
            eta_text = " | ETA: " + qchaves::system::format_duration(static_cast<std::uint64_t>(remaining));
        }
        std::cout << qchaves::system::make_progress_bar(ratio)
                  << ' ' << static_cast<int>(ratio * 100.0)
                  << "% | Table: 100% | Keys: " << qchaves::system::format_key_count(static_cast<double>(processed))
                  << " | Rate: " << qchaves::system::format_rate(rate)
                  << eta_text << '\n';
        return;
    }

    std::cout << "[+] Table: 100% | Keys: " << qchaves::system::format_key_count(static_cast<double>(processed))
              << " | Rate: " << qchaves::system::format_rate(rate)
              << " | Time: " << qchaves::system::format_duration(seconds) << '\n';
}

void print_kangaroo_stats(std::uint64_t processed,
                          const std::chrono::steady_clock::duration& elapsed) {
    const auto seconds = std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()));
    const double rate = static_cast<double>(processed) / static_cast<double>(seconds);
    std::cout << "[Hops: " << qchaves::system::format_key_count(static_cast<double>(processed))
              << " | Dist: 0 | Rate: " << qchaves::system::format_rate(rate)
              << " | Time: " << qchaves::system::format_duration(seconds) << "]\n";
}

std::optional<int> run_address_parallel(const qchaves::system::AddressOptions& options,
                                        const qchaves::system::TuneProfile& tune,
                                        const qchaves::system::TargetLoadResult& targets,
                                        const std::filesystem::path& checkpoint_path,
                                        const SearchRange& original_range,
                                        CheckpointState& checkpoint,
                                        bool checkpoint_enabled) {
    std::uint64_t width_u64 = 0;
    if (!can_parallelize_address_range(original_range, tune.threads, width_u64)) {
        return std::nullopt;
    }
    if (options.mode == qchaves::system::SearchMode::random && options.limit == 0) {
        return std::nullopt;
    }

    AddressParallelState state;
    state.active_workers.store(tune.threads, std::memory_order_relaxed);
    state.random_checkpoint.store(0u, std::memory_order_relaxed);

    const auto started = std::chrono::steady_clock::now();
    auto last_stats = started;
    auto last_checkpoint = started;
    const std::uint64_t chunk_size = std::max<std::uint64_t>(1u, static_cast<std::uint64_t>(tune.batch_size));
    const std::uint32_t thread_count = std::max<std::uint32_t>(1u, tune.threads);
    const std::uint32_t forward_threads = options.mode == qchaves::system::SearchMode::both
        ? std::max<std::uint32_t>(1u, thread_count / 2u)
        : thread_count;
    const std::uint32_t backward_threads = options.mode == qchaves::system::SearchMode::both
        ? std::max<std::uint32_t>(1u, thread_count - forward_threads)
        : 0u;
    const std::uint64_t forward_width = options.mode == qchaves::system::SearchMode::both
        ? ((width_u64 + 1u) / 2u)
        : width_u64;
    const std::uint64_t backward_width = options.mode == qchaves::system::SearchMode::both
        ? (width_u64 / 2u)
        : 0u;

    std::cout << "[+] Address parallel workers: " << thread_count
              << " | mode: " << qchaves::system::to_string(options.mode)
              << " | chunk: " << chunk_size << '\n';

    auto worker = [&](std::uint32_t thread_id, bool backward_lane) {
        std::mt19937_64 rng(std::random_device{}() ^ (static_cast<std::uint64_t>(thread_id) << 32u));
        while (!state.stop.load(std::memory_order_relaxed) && !interrupt_requested()) {
            if (options.limit > 0 &&
                state.processed_total.load(std::memory_order_relaxed) >= options.limit) {
                state.stop.store(true, std::memory_order_relaxed);
                break;
            }

            std::uint64_t local_begin = 0;
            std::uint64_t local_end = 0;
            bool valid_chunk = true;
            bool chunk_is_backward = backward_lane;

            if (options.mode == qchaves::system::SearchMode::sequential) {
                const std::uint64_t offset = state.next_forward_offset.fetch_add(chunk_size, std::memory_order_relaxed);
                if (offset >= width_u64) {
                    break;
                }
                local_begin = offset;
                local_end = std::min(width_u64 - 1u, local_begin + chunk_size - 1u);
            } else if (options.mode == qchaves::system::SearchMode::backward) {
                const std::uint64_t offset = state.next_backward_offset.fetch_add(chunk_size, std::memory_order_relaxed);
                if (offset >= width_u64) {
                    break;
                }
                const std::uint64_t span = std::min(chunk_size, width_u64 - offset);
                local_begin = offset;
                local_end = offset + (span - 1u);
                chunk_is_backward = true;
            } else if (options.mode == qchaves::system::SearchMode::both) {
                if (backward_lane) {
                    if (backward_width == 0) {
                        break;
                    }
                    const std::uint64_t offset = state.next_backward_offset.fetch_add(chunk_size, std::memory_order_relaxed);
                    if (offset >= backward_width) {
                        break;
                    }
                    const std::uint64_t span = std::min(chunk_size, backward_width - offset);
                    local_begin = offset;
                    local_end = offset + (span - 1u);
                    chunk_is_backward = true;
                } else {
                    if (forward_width == 0) {
                        break;
                    }
                    const std::uint64_t offset = state.next_forward_offset.fetch_add(chunk_size, std::memory_order_relaxed);
                    if (offset >= forward_width) {
                        break;
                    }
                    local_begin = offset;
                    local_end = std::min(forward_width - 1u, local_begin + chunk_size - 1u);
                    chunk_is_backward = false;
                }
            } else {
                if (width_u64 == 0) {
                    break;
                }
                const std::uint64_t sample = std::uniform_int_distribution<std::uint64_t>(0, width_u64 - 1u)(rng);
                local_begin = sample;
                local_end = local_begin;
                state.random_checkpoint.store(sample, std::memory_order_relaxed);
                valid_chunk = true;
                chunk_is_backward = false;
            }

            if (!valid_chunk) {
                break;
            }

            if (chunk_is_backward) {
                for (std::uint64_t current = local_end;; --current) {
                    if (state.stop.load(std::memory_order_relaxed) || interrupt_requested()) {
                        break;
                    }
                    if (options.limit > 0 &&
                        state.processed_total.load(std::memory_order_relaxed) >= options.limit) {
                        state.stop.store(true, std::memory_order_relaxed);
                        break;
                    }
                    qchaves::core::DerivedKeyInfo key_info;
                    const BigInt candidate = original_range.end - BigInt(current);
                    if (!qchaves::core::derive_key_info(candidate, key_info)) {
                        state.stop.store(true, std::memory_order_relaxed);
                        break;
                    }
                    state.processed_total.fetch_add(1u, std::memory_order_relaxed);
                    state.processed_backward.fetch_add(1u, std::memory_order_relaxed);
                    for (const auto& target : targets.entries) {
                        if (target_matches(target, options.type, key_info)) {
                            store_parallel_hit(state, thread_id, key_info);
                            break;
                        }
                    }
                    if (state.stop.load(std::memory_order_relaxed) || current == local_begin) {
                        break;
                    }
                }
            } else {
                for (std::uint64_t current = local_begin; current <= local_end; ++current) {
                    if (state.stop.load(std::memory_order_relaxed) || interrupt_requested()) {
                        break;
                    }
                    if (options.limit > 0 &&
                        state.processed_total.load(std::memory_order_relaxed) >= options.limit) {
                        state.stop.store(true, std::memory_order_relaxed);
                        break;
                    }
                    qchaves::core::DerivedKeyInfo key_info;
                    const BigInt candidate = original_range.start + BigInt(current);
                    if (!qchaves::core::derive_key_info(candidate, key_info)) {
                        state.stop.store(true, std::memory_order_relaxed);
                        break;
                    }
                    state.processed_total.fetch_add(1u, std::memory_order_relaxed);
                    state.processed_forward.fetch_add(1u, std::memory_order_relaxed);
                    for (const auto& target : targets.entries) {
                        if (target_matches(target, options.type, key_info)) {
                            store_parallel_hit(state, thread_id, key_info);
                            break;
                        }
                    }
                    if (state.stop.load(std::memory_order_relaxed) || current == local_end) {
                        break;
                    }
                }
            }
        }

        state.active_workers.fetch_sub(1u, std::memory_order_relaxed);
    };

    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    std::uint32_t thread_id = 1;
    if (options.mode == qchaves::system::SearchMode::both) {
        for (std::uint32_t i = 0; i < forward_threads; ++i) {
            workers.emplace_back(worker, thread_id++, false);
        }
        for (std::uint32_t i = 0; i < backward_threads; ++i) {
            workers.emplace_back(worker, thread_id++, true);
        }
    } else {
        for (std::uint32_t i = 0; i < thread_count; ++i) {
            workers.emplace_back(worker, thread_id++, options.mode == qchaves::system::SearchMode::backward);
        }
    }

    while (state.active_workers.load(std::memory_order_relaxed) > 0) {
        if (interrupt_requested()) {
            state.stop.store(true, std::memory_order_relaxed);
        }
        if (options.limit > 0 &&
            state.processed_total.load(std::memory_order_relaxed) >= options.limit) {
            state.stop.store(true, std::memory_order_relaxed);
        }

        const auto now = std::chrono::steady_clock::now();
        if (checkpoint_enabled &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            update_checkpoint_current(checkpoint, compute_parallel_checkpoint_current(options, original_range, tune, state, width_u64));
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                state.stop.store(true, std::memory_order_relaxed);
                for (auto& worker_thread : workers) {
                    if (worker_thread.joinable()) {
                        worker_thread.join();
                    }
                }
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            last_checkpoint = now;
        }
        if (now - last_stats >= std::chrono::seconds(30)) {
            print_address_stats(options.mode,
                                state.processed_total.load(std::memory_order_relaxed),
                                state.processed_forward.load(std::memory_order_relaxed),
                                state.processed_backward.load(std::memory_order_relaxed),
                                now - started,
                                original_range);
            last_stats = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    for (auto& worker_thread : workers) {
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    if (interrupt_requested()) {
        update_checkpoint_current(checkpoint, compute_parallel_checkpoint_current(options, original_range, tune, state, width_u64));
        return finalize_interrupted_run(options, checkpoint_path, checkpoint, checkpoint_to_bigint(checkpoint.current));
    }

    if (state.hit_found) {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        update_checkpoint_current(checkpoint, state.hit.key_info.private_key);
        if (checkpoint_enabled) {
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
        }
        print_found("address", options.target_path, options.type, state.hit.key_info, state.hit.thread_id, elapsed);
        append_found_record("address", options.target_path, options.type, state.hit.key_info, state.hit.thread_id, elapsed);
        return 0;
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    update_checkpoint_current(checkpoint, compute_parallel_checkpoint_current(options, original_range, tune, state, width_u64));
    if (checkpoint_enabled) {
        std::string checkpoint_error;
        if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
            std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
            return 1;
        }
    }
    print_address_stats(options.mode,
                        state.processed_total.load(std::memory_order_relaxed),
                        state.processed_forward.load(std::memory_order_relaxed),
                        state.processed_backward.load(std::memory_order_relaxed),
                        elapsed,
                        original_range);
    std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
    return 0;
}

}  // namespace

int run_address(const qchaves::system::AddressOptions& options) {
    ScopedInterruptHandler interrupt_handler;
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

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("address", range.puzzle_bits));
    const auto resume = load_resume_state(checkpoint_path);
    auto checkpoint = build_checkpoint("address", options.mode, options.type, tune);
    bool resume_completed = false;
    const SearchRange original_range = range;
    checkpoint_bigint(range.start, checkpoint.range_start);
    checkpoint_bigint(range.end, checkpoint.range_end);
    checkpoint_bigint(range.start, checkpoint.current);
    apply_resume_for_range("address", options.mode, options.type, resume, original_range, range, checkpoint, resume_completed);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }
    if (resume_completed) {
        std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
        return 0;
    }

    if (const auto parallel_result = run_address_parallel(options,
                                                          tune,
                                                          targets,
                                                          checkpoint_path,
                                                          range,
                                                          checkpoint,
                                                          options.checkpoint_enabled);
        parallel_result.has_value()) {
        return parallel_result.value();
    }

    if (tune.threads > 1) {
        std::cout << "[i] Paralelizacao real indisponivel para este range/modo; usando fallback single-thread." << '\n';
    }

    std::mt19937_64 rng(std::random_device{}());
    BigInt forward = range.start;
    BigInt backward = range.end;
    BigInt last_processed = range.start;
    bool use_forward = true;
    std::uint64_t processed = estimate_processed_offset(options.mode, original_range, range);
    std::uint64_t processed_forward = options.mode == qchaves::system::SearchMode::backward ? 0 : processed;
    std::uint64_t processed_backward = options.mode == qchaves::system::SearchMode::backward ? processed : 0;
    const auto started = std::chrono::steady_clock::now();
    auto last_stats = started;
    auto last_checkpoint = started;

    while (true) {
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }
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
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (options.checkpoint_enabled &&
                !persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            print_found("address", options.target_path, options.type, key_info, 1u, elapsed);
            append_found_record("address", options.target_path, options.type, key_info, 1u, elapsed);
            return 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }
        if (options.checkpoint_enabled &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            last_checkpoint = now;
        }
        if (now - last_stats >= std::chrono::seconds(30)) {
            print_address_stats(options.mode, processed, processed_forward, processed_backward, now - started, original_range);
            last_stats = now;
        }
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    update_checkpoint_current(checkpoint, last_processed);
    if (options.checkpoint_enabled) {
        std::string checkpoint_error;
        if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
            std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
            return 1;
        }
    }
    print_address_stats(options.mode, processed, processed_forward, processed_backward, elapsed, original_range);
    std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
    return 0;
}

int run_bsgs(const qchaves::system::BsgsOptions& options) {
    ScopedInterruptHandler interrupt_handler;
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

    SearchRange range;
    std::string range_error;
    if (!resolve_bsgs_range(options, range, range_error)) {
        std::cerr << "[E] " << range_error << '\n';
        return 1;
    }
    std::cout << "[+] Range start: " << qchaves::core::bigint_to_hex(range.start) << '\n';
    std::cout << "[+] Range end:   " << qchaves::core::bigint_to_hex(range.end) << '\n';
    if (tune.threads > 1) {
        std::cout << "[i] Backend BSGS de referencia usando 1 thread efetiva nesta fase." << '\n';
    }

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("bsgs", options.bits));
    const auto resume = load_resume_state(checkpoint_path);
    auto checkpoint = build_checkpoint("bsgs", qchaves::system::SearchMode::sequential, options.type, tune);
    bool resume_completed = false;
    const SearchRange original_range = range;
    checkpoint_bigint(range.start, checkpoint.range_start);
    checkpoint_bigint(range.end, checkpoint.range_end);
    checkpoint_bigint(range.start, checkpoint.current);
    apply_resume_for_range("bsgs",
                           qchaves::system::SearchMode::sequential,
                           options.type,
                           resume,
                           original_range,
                           range,
                           checkpoint,
                           resume_completed);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }
    if (resume_completed) {
        std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
        return 0;
    }

    BigInt current = range.start;
    BigInt last_processed = range.start;
    std::uint64_t processed = estimate_processed_offset(qchaves::system::SearchMode::sequential, original_range, range);
    const auto started = std::chrono::steady_clock::now();
    auto last_stats = started;
    auto last_checkpoint = started;

    while (current <= range.end) {
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }

        qchaves::core::DerivedKeyInfo key_info;
        if (!qchaves::core::derive_key_info(current, key_info)) {
            std::cerr << "[E] Chave privada invalida no range atual" << '\n';
            return 1;
        }

        last_processed = current;
        ++processed;
        for (const auto& target : targets.entries) {
            if (!target_matches(target, options.type, key_info)) {
                continue;
            }
            const auto elapsed = std::chrono::steady_clock::now() - started;
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (options.checkpoint_enabled &&
                !persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            print_found("bsgs", options.target_path, options.type, key_info, 1u, elapsed);
            append_found_record("bsgs", options.target_path, options.type, key_info, 1u, elapsed);
            return 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }
        if (options.checkpoint_enabled &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            last_checkpoint = now;
        }
        if (now - last_stats >= std::chrono::seconds(30)) {
            print_bsgs_stats(processed, now - started, original_range);
            last_stats = now;
        }

        ++current;
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    update_checkpoint_current(checkpoint, last_processed);
    if (options.checkpoint_enabled) {
        std::string checkpoint_error;
        if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
            std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
            return 1;
        }
    }
    print_bsgs_stats(processed, elapsed, original_range);
    std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
    return 0;
}

int run_kangaroo(const qchaves::system::KangarooOptions& options) {
    ScopedInterruptHandler interrupt_handler;
    const auto targets = qchaves::system::load_targets(options.target_path, true);
    print_warnings(targets);
    if (targets.entries.empty()) {
        std::cerr << "[E] Nenhuma public key valida carregada" << '\n';
        return 1;
    }

    const auto hardware = qchaves::system::detect_hardware();
    const auto tune = qchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    print_bootstrap("kangaroo", options, targets, hardware, tune);

    SearchRange range;
    std::string range_error;
    if (!parse_range_expression(options.range, range, range_error)) {
        std::cerr << "[E] " << range_error << '\n';
        return 1;
    }
    std::cout << "[+] Range start: " << qchaves::core::bigint_to_hex(range.start) << '\n';
    std::cout << "[+] Range end:   " << qchaves::core::bigint_to_hex(range.end) << '\n';
    if (tune.threads > 1) {
        std::cout << "[i] Backend Kangaroo de referencia usando 1 thread efetiva nesta fase." << '\n';
    }

    const auto checkpoint_path = options.checkpoint_path.value_or(qchaves::system::default_checkpoint_path("kangaroo"));
    const auto resume = load_resume_state(checkpoint_path);
    auto checkpoint = build_checkpoint("kangaroo", qchaves::system::SearchMode::sequential, qchaves::system::SearchType::both, tune);
    bool resume_completed = false;
    const SearchRange original_range = range;
    checkpoint_bigint(range.start, checkpoint.range_start);
    checkpoint_bigint(range.end, checkpoint.range_end);
    checkpoint_bigint(range.start, checkpoint.current);
    apply_resume_for_range("kangaroo",
                           qchaves::system::SearchMode::sequential,
                           qchaves::system::SearchType::both,
                           resume,
                           original_range,
                           range,
                           checkpoint,
                           resume_completed);
    if (const int checkpoint_status = finalize_checkpoint(options, checkpoint_path, checkpoint); checkpoint_status != 0) {
        return checkpoint_status;
    }
    if (resume_completed) {
        std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
        return 0;
    }

    BigInt current = range.start;
    BigInt last_processed = range.start;
    std::uint64_t processed = estimate_processed_offset(qchaves::system::SearchMode::sequential, original_range, range);
    const auto started = std::chrono::steady_clock::now();
    auto last_stats = started;
    auto last_checkpoint = started;

    while (current <= range.end) {
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }

        qchaves::core::DerivedKeyInfo key_info;
        if (!qchaves::core::derive_key_info(current, key_info)) {
            std::cerr << "[E] Chave privada invalida no range atual" << '\n';
            return 1;
        }

        last_processed = current;
        ++processed;
        for (const auto& target : targets.entries) {
            if (!target_matches(target, qchaves::system::SearchType::both, key_info)) {
                continue;
            }
            const auto elapsed = std::chrono::steady_clock::now() - started;
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (options.checkpoint_enabled &&
                !persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            print_found("kangaroo", options.target_path, qchaves::system::SearchType::both, key_info, 1u, elapsed);
            append_found_record("kangaroo", options.target_path, qchaves::system::SearchType::both, key_info, 1u, elapsed);
            return 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (interrupt_requested()) {
            return finalize_interrupted_run(options, checkpoint_path, checkpoint, last_processed);
        }
        if (options.checkpoint_enabled &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            update_checkpoint_current(checkpoint, last_processed);
            std::string checkpoint_error;
            if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
                std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
                return 1;
            }
            last_checkpoint = now;
        }
        if (now - last_stats >= std::chrono::seconds(30)) {
            print_kangaroo_stats(processed, now - started);
            last_stats = now;
        }

        ++current;
    }

    const auto elapsed = std::chrono::steady_clock::now() - started;
    update_checkpoint_current(checkpoint, last_processed);
    if (options.checkpoint_enabled) {
        std::string checkpoint_error;
        if (!persist_checkpoint(checkpoint_path, checkpoint, checkpoint_error)) {
            std::cerr << "[E] Falha ao salvar checkpoint: " << checkpoint_error << '\n';
            return 1;
        }
    }
    print_kangaroo_stats(processed, elapsed);
    std::cout << "[+] Busca finalizada sem correspondencias" << '\n';
    return 0;
}

}  // namespace qchaves::engine
