/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições e interfaces para detecção de hardware.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#pragma once

#include "system/types.hpp"

namespace bchaves::system {

HardwareInfo detect_hardware();
TuneProfile tune_for(const HardwareInfo& hardware, AutoTuneProfile profile, std::uint32_t requested_threads, std::uint32_t requested_table_k = 0);
bool pin_thread_to_core(std::uint32_t core_id);
bool pin_thread_to_node(std::uint32_t node_id);
void pin_all_threads(std::uint32_t num_threads);

// HugePages allocation — transparente com fallback para alocação normal
void* allocate_huge_pages(std::size_t size_bytes);
void  free_huge_pages(void* ptr, std::size_t size_bytes);

}  // namespace bchaves::system
