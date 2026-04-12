#include "system/checkpoint.hpp"

#include "core/hash.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace bchaves::system {
namespace {

constexpr char kMagic[] = {'B', 'C', 'H', 'V'};
constexpr std::uint32_t kVersion = 4u;

void append_u32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xffu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void append_u64(std::vector<std::uint8_t>& buffer, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        buffer.push_back(static_cast<std::uint8_t>((value >> (8u * i)) & 0xffu));
    }
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& buffer, std::size_t& offset) {
    const std::uint32_t value = static_cast<std::uint32_t>(buffer[offset]) |
                                (static_cast<std::uint32_t>(buffer[offset + 1]) << 8u) |
                                (static_cast<std::uint32_t>(buffer[offset + 2]) << 16u) |
                                (static_cast<std::uint32_t>(buffer[offset + 3]) << 24u);
    offset += 4;
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& buffer, std::size_t& offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(buffer[offset + i]) << (8u * i);
    }
    offset += 8;
    return value;
}

void append_string(std::vector<std::uint8_t>& buffer, const std::string& value) {
    append_u32(buffer, static_cast<std::uint32_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

bool read_string(const std::vector<std::uint8_t>& buffer, std::size_t& offset, std::string& value) {
    if (offset + 4 > buffer.size()) {
        return false;
    }
    const std::uint32_t length = read_u32(buffer, offset);
    if (offset + length > buffer.size()) {
        return false;
    }
    value.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
    offset += length;
    return true;
}

}  // namespace

std::filesystem::path default_checkpoint_path(const std::string& algorithm, const std::optional<std::uint32_t>& bits) {
    if (bits.has_value()) {
        return std::filesystem::path(algorithm + "_bit" + std::to_string(bits.value()) + ".ckp");
    }
    return std::filesystem::path(algorithm + "_default.ckp");
}

bool save_checkpoint(const std::filesystem::path& file, const CheckpointState& state, std::string& error) {
    std::vector<std::uint8_t> buffer;
    buffer.insert(buffer.end(), std::begin(kMagic), std::end(kMagic));
    append_u32(buffer, kVersion);
    append_string(buffer, state.algorithm);
    append_u32(buffer, static_cast<std::uint32_t>(state.mode));
    append_u32(buffer, static_cast<std::uint32_t>(state.type));
    append_u32(buffer, state.threads);
    append_u32(buffer, state.batch_size);
    append_u64(buffer, state.timestamp);
    buffer.insert(buffer.end(), state.range_start.begin(), state.range_start.end());
    buffer.insert(buffer.end(), state.range_end.begin(), state.range_end.end());
    buffer.insert(buffer.end(), state.current.begin(), state.current.end());
    buffer.insert(buffer.end(), state.current_aux.begin(), state.current_aux.end());
    append_u64(buffer, state.progress_primary);
    append_u64(buffer, state.progress_secondary);
    append_u64(buffer, state.random_seed);
    append_u32(buffer, bchaves::core::crc32(buffer));

    std::ofstream out(file, std::ios::binary);
    if (!out) {
        error = "nao foi possivel abrir arquivo de checkpoint para escrita";
        return false;
    }
    out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!out) {
        error = "falha ao gravar checkpoint";
        return false;
    }
    return true;
}

bool load_checkpoint(const std::filesystem::path& file, CheckpointState& state, std::string& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        error = "nao foi possivel abrir checkpoint";
        return false;
    }

    std::vector<std::uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buffer.size() < 12u) {
        error = "checkpoint muito pequeno";
        return false;
    }

    const std::uint32_t stored_crc = static_cast<std::uint32_t>(buffer[buffer.size() - 4]) |
                                     (static_cast<std::uint32_t>(buffer[buffer.size() - 3]) << 8u) |
                                     (static_cast<std::uint32_t>(buffer[buffer.size() - 2]) << 16u) |
                                     (static_cast<std::uint32_t>(buffer[buffer.size() - 1]) << 24u);
    const std::vector<std::uint8_t> payload(buffer.begin(), buffer.end() - 4);
    if (bchaves::core::crc32(payload) != stored_crc) {
        error = "checkpoint com CRC invalido";
        return false;
    }
    if (!std::equal(std::begin(kMagic), std::end(kMagic), payload.begin())) {
        error = "magic invalido";
        return false;
    }

    std::size_t offset = 4;
    const std::uint32_t version = read_u32(payload, offset);
    if (version != 3u && version != kVersion) {
        error = "versao de checkpoint nao suportada";
        return false;
    }
    state = {};
    state.format_version = version;
    if (!read_string(payload, offset, state.algorithm)) {
        error = "checkpoint truncado ao ler algoritmo";
        return false;
    }
    state.mode = static_cast<SearchMode>(read_u32(payload, offset));
    state.type = static_cast<SearchType>(read_u32(payload, offset));
    state.threads = read_u32(payload, offset);
    state.batch_size = read_u32(payload, offset);
    state.timestamp = read_u64(payload, offset);
    if (offset + 96 > payload.size()) {
        error = "checkpoint truncado ao ler ranges";
        return false;
    }
    std::memcpy(state.range_start.data(), payload.data() + offset, state.range_start.size());
    offset += state.range_start.size();
    std::memcpy(state.range_end.data(), payload.data() + offset, state.range_end.size());
    offset += state.range_end.size();
    std::memcpy(state.current.data(), payload.data() + offset, state.current.size());
    offset += state.current.size();
    if (version >= 4u) {
        if (offset + state.current_aux.size() + 24u > payload.size()) {
            error = "checkpoint truncado ao ler estado extendido";
            return false;
        }
        std::memcpy(state.current_aux.data(), payload.data() + offset, state.current_aux.size());
        offset += state.current_aux.size();
        state.progress_primary = read_u64(payload, offset);
        state.progress_secondary = read_u64(payload, offset);
        state.random_seed = read_u64(payload, offset);
    }
    return true;
}

}  // namespace bchaves::system
