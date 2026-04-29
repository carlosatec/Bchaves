/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação da orquestração global de motores e utilitários.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "system/format.hpp"
#include "system/io.hpp"

namespace bchaves::engine {

void report_found(const bchaves::core::DerivedKeyInfo& info, const std::string& context, bool persist) {
    if (persist) {
        bchaves::system::save_found_result(info, context);
    }
    
    bchaves::system::print_success_report(info, context);
}

}  // namespace bchaves::engine
