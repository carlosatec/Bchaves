#pragma once

#include "system/types.hpp"

#include <filesystem>

namespace qchaves::system {

TargetType detect_type(const std::string& line);
TargetLoadResult load_targets(const std::filesystem::path& file, bool require_pubkeys_only = false);
TargetLoadResult load_target_inline(const std::string& input, bool require_pubkeys_only = false);

}  // namespace qchaves::system
