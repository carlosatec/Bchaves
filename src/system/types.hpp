#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace qchaves::system {

enum class TargetType {
    invalid = 0,
    address_btc,
    pubkey_compress,
    pubkey_uncompress,
};

enum class SearchMode {
    sequential = 0,
    backward,
    both,
    random,
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
    bool checkpoint_enabled = true;
    bool help = false;
};

struct AddressOptions : CommonOptions {
    SearchMode mode = SearchMode::sequential;
    SearchType type = SearchType::both;
    std::string range_start;
    std::string range_end;
    std::uint64_t limit = 0;
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
    std::string algorithm;
    SearchMode mode = SearchMode::sequential;
    SearchType type = SearchType::both;
    std::uint32_t threads = 1;
    std::uint32_t batch_size = 1024;
    std::uint64_t timestamp = 0;
    std::array<std::uint8_t, 32> range_start{};
    std::array<std::uint8_t, 32> range_end{};
    std::array<std::uint8_t, 32> current{};
};

std::string to_string(TargetType value);
std::string to_string(SearchMode value);
std::string to_string(SearchType value);
std::string to_string(AutoTuneProfile value);

}  // namespace qchaves::system
