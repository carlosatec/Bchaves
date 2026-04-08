#pragma once

#include "system/types.hpp"

#include <filesystem>
#include <optional>

namespace qchaves::system {

std::filesystem::path default_checkpoint_path(const std::string& algorithm, const std::optional<std::uint32_t>& bits = std::nullopt);
bool save_checkpoint(const std::filesystem::path& file, const CheckpointState& state, std::string& error);
bool load_checkpoint(const std::filesystem::path& file, CheckpointState& state, std::string& error);

}  // namespace qchaves::system
