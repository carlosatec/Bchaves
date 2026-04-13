/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de persistência de descobertas e E/S seguro.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "system/io.hpp"
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace bchaves::system {

static std::mutex g_io_mutex;

bool save_found_result(const bchaves::core::DerivedKeyInfo& info, const std::string& context) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    
    std::ofstream file("found.txt", std::ios::app);
    if (!file.is_open()) {
        return false;
    }

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    file << "===============================================================\n";
    file << "Bchaves: Key Found!\n";
    file << "Date:    " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "\n";
    file << "Context: " << context << "\n";
    file << "---------------------------------------------------------------\n";
    file << "Private (Hex): " << bchaves::core::bigint_to_hex(info.private_key) << "\n";
    file << "Private (Dec): " << bchaves::core::bigint_to_decimal(info.private_key) << "\n";
    file << "WIF (Comp):    " << info.wif_compressed << "\n";
    file << "Address:       " << info.address_compressed << "\n";
    file << "===============================================================\n\n";

    file.flush();
    file.close();
    return true;
}

}  // namespace bchaves::system
