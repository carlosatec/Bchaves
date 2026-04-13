/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições de tipos comuns, enums e estruturas de dados do sistema.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bchaves::system {

enum class TargetType {
    invalid = 0,
    address_btc, // Legacy P2PKH (1...)
    address_p2sh, // Nested SegWit (3...)
    address_bech32, // Native SegWit (bc1...)
    hash160,       // 20 bytes hex payload
    pubkey_compress,
    pubkey_uncompress,
};

enum class SearchMode {
    sequential = 0,
    backward,
    both,
    hybrid,
};

enum class SearchType {
    compress = 0,
    uncompress,
    both,
};

enum class AutoTuneProfile {
    safe = 0,
    balanced,
    max,
};

enum class Secp256k1BackendPreference {
    auto_select = 0,
    portable,
    external,
};

enum CPUFeature : std::uint32_t {
    cpu_none = 0,
    cpu_sse4 = 1u << 0u,
    cpu_avx2 = 1u << 1u,
    cpu_avx512 = 1u << 2u,
    cpu_sha_ni = 1u << 3u,
    cpu_bmi2 = 1u << 4u,
};

struct TargetEntry {
    std::size_t line_number = 0;
    std::string raw;
    TargetType type = TargetType::invalid;
    std::vector<std::uint8_t> payload;
};

struct LoadWarning {
    std::size_t line_number = 0;
    std::string message;
};

struct TargetLoadResult {
    std::vector<TargetEntry> entries;
    std::vector<LoadWarning> warnings;
};

struct HardwareInfo {
    std::uint32_t num_cores = 1;
    std::uint32_t num_physical_cores = 1;
    std::uint64_t ram_total = 0;
    std::uint64_t ram_available = 0;
    std::uint32_t l1_cache = 0;
    std::uint32_t l2_cache = 0;
    std::uint32_t l3_cache = 0;
    std::uint32_t features = cpu_none;
    bool is_numa = false;
};

struct TuneProfile {
    std::uint32_t threads = 1;
    std::uint32_t batch_size = 1024;
    std::uint32_t table_k = 512;
};

struct CommonOptions {
    std::filesystem::path target_path;
    std::optional<std::filesystem::path> checkpoint_path;
    std::uint32_t checkpoint_interval_seconds = 300;
    std::uint32_t threads = 0;
    AutoTuneProfile auto_tune = AutoTuneProfile::balanced;
    Secp256k1BackendPreference secp256k1_backend = Secp256k1BackendPreference::auto_select;
    bool checkpoint_enabled = true;
    bool benchmark = false;
    bool help = false;
};

struct AddressOptions : CommonOptions {
    SearchMode mode = SearchMode::sequential;
    SearchType type = SearchType::both;
    std::uint64_t limit = 0;
    std::uint32_t bits = 0;
    std::uint32_t chunk_k = 0;   // -k: chunk_size = 1024 * chunk_k (min 1M)
};

struct BsgsOptions : CommonOptions {
    std::uint32_t bits = 0;
    std::uint32_t table_k = 0;
    SearchType type = SearchType::both;
};

struct KangarooOptions : CommonOptions {
    std::string range;
};

struct CheckpointState {
    std::uint32_t format_version = 5;
    std::string algorithm;
    SearchMode mode = SearchMode::sequential;
    SearchType type = SearchType::both;
    std::uint32_t threads = 1;
    std::uint32_t batch_size = 1024;
    std::uint64_t timestamp = 0;
    std::array<std::uint8_t, 32> range_start{};
    std::array<std::uint8_t, 32> range_end{};
    std::array<std::uint8_t, 32> current{};
    std::array<std::uint8_t, 32> current_aux{};
    std::uint64_t progress_primary = 0;
    std::uint64_t progress_secondary = 0;
    // v5: campos dedicados para modo hybrid
    std::uint64_t hybrid_chunk_counter = 0;
    std::uint64_t hybrid_chunk_step    = 0;
    std::uint64_t hybrid_chunk_size    = 0;
    std::uint64_t hybrid_total_chunks  = 0;
};

std::string to_string(TargetType value);
std::string to_string(SearchMode value);
std::string to_string(SearchType value);
std::string to_string(AutoTuneProfile value);
std::string to_string(Secp256k1BackendPreference value);

}  // namespace bchaves::system
