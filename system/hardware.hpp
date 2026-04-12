#pragma once

#include "system/types.hpp"

namespace bchaves::system {

HardwareInfo detect_hardware();
TuneProfile tune_for(const HardwareInfo& hardware, AutoTuneProfile profile, std::uint32_t requested_threads, std::uint32_t requested_table_k = 0);
bool pin_thread_to_core(std::uint32_t core_id);
void pin_all_threads(std::uint32_t num_threads);

}  // namespace bchaves::system
