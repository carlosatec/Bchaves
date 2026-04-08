#pragma once

#include "system/types.hpp"

namespace qchaves::system {

HardwareInfo detect_hardware();
TuneProfile tune_for(const HardwareInfo& hardware, AutoTuneProfile profile, std::uint32_t requested_threads, std::uint32_t requested_table_k = 0);

}  // namespace qchaves::system
