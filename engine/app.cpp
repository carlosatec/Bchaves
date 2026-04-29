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
#include "core/secp256k1.hpp"
#include "system/format.hpp"
#include "system/io.hpp"

namespace bchaves::engine {

bool configure_secp256k1_backend(bchaves::system::Secp256k1BackendPreference preference, std::string& error) {
    bchaves::core::Secp256k1BackendKind backend = bchaves::core::Secp256k1BackendKind::auto_select;
    switch (preference) {
        case bchaves::system::Secp256k1BackendPreference::auto_select:
            backend = bchaves::core::Secp256k1BackendKind::auto_select;
            break;
        case bchaves::system::Secp256k1BackendPreference::portable:
            backend = bchaves::core::Secp256k1BackendKind::portable;
            break;
        case bchaves::system::Secp256k1BackendPreference::external:
            backend = bchaves::core::Secp256k1BackendKind::external;
            break;
    }
    return bchaves::core::select_secp256k1_backend(backend, error);
}

void report_found(const bchaves::core::DerivedKeyInfo& info, const std::string& context, bool persist) {
    if (persist) {
        bchaves::system::save_found_result(info, context);
    }
    
    bchaves::system::print_success_report(info, context);
}

}  // namespace bchaves::engine
