/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Algoritmo Pollard's Kangaroo (Architectural Fleet Model).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "core/secp256k1.hpp"
#include "core/hash.hpp"
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <csignal>
#include <fstream>

namespace bchaves::engine {
namespace {

std::atomic<bool> g_stop_requested{false};
void handle_sig(int) { g_stop_requested = true; }

struct KangarooTrap {
    bchaves::core::BigInt distance;
    bool is_wild;
};

struct TrapShard {
    std::mutex mtx;
    std::unordered_map<std::uint64_t, KangarooTrap> table; // HashX -> Trap
};

struct Kangaroo {
    bchaves::core::Secp256k1Point point;
    bchaves::core::BigInt distance;
    bool is_wild;
};

struct Jump {
    bchaves::core::Secp256k1Point point;
    bchaves::core::BigInt distance;
};

// Global Jump Table
std::array<Jump, 64> g_jump_table;

void init_jump_table() {
    bchaves::core::BigInt d(1);
    for(int i=0; i<64; ++i) {
        g_jump_table[i].distance = d;
        g_jump_table[i].point = bchaves::core::secp256k1_multiply(d);
        d = d << 1;
    }
}

bool is_distinguished(const bchaves::core::BigInt& x, std::uint32_t bits) {
    // Check if leading N bits are zero (high bits)
    if (bits == 0) return true;
    std::uint32_t limb_idx = (255 - bits) / 64;
    std::uint32_t bit_offset = 63 - (255 - bits) % 64;
    if (limb_idx >= 4) return x.is_zero();
    std::uint64_t mask = (bit_offset == 63) ? ~0ULL : (((1ULL << (bit_offset + 1)) - 1ULL));
    return (x.limbs[limb_idx] & mask) == 0;
}

} // namespace

int run_kangaroo(const bchaves::system::KangarooOptions& options) {
    auto hardware = bchaves::system::detect_hardware();
    if (options.help) {
        // Se foi --list-hardware via CommonOptions (flag modificada no cli.cpp)
        // No CLI atual, setamos help=true para forçar a parada.
        std::cout << "[*] Hardware Detectado:\n"
                  << "    Cores: " << hardware.num_cores << " (Fisicos: " << hardware.num_physical_cores << ")\n"
                  << "    RAM: " << (hardware.ram_total / (1024*1024*1024)) << " GB (Livre: " << (hardware.ram_available / (1024*1024*1024)) << " GB)\n"
                  << "    Cache L3: " << (hardware.l3_cache / (1024*1024)) << " MB\n"
                  << "    Features: " << (hardware.features & bchaves::system::cpu_avx2 ? "AVX2 " : "")
                                     << (hardware.features & bchaves::system::cpu_bmi2 ? "BMI2 " : "") 
                                     << (hardware.features & bchaves::system::cpu_sha_ni ? "SHA-NI " : "") << "\n";
        return 0;
    }

    std::signal(SIGINT, handle_sig);
    std::cout << "[+] Iniciando Kangaroo (Architectural Fleet Model)\n";
    
    bchaves::core::BigInt range_start, range_end;
    if (options.range.find("bits:") == 0) {
        uint32_t bits = std::stoul(options.range.substr(5));
        std::cout << "[*] Modo Bits Detectado: " << bits << "\n";
        // range_start = 2^(bits-1), range_end = 2^bits - 1
        range_start = bchaves::core::BigInt(1) << (bits - 1);
        range_end = (bchaves::core::BigInt(1) << bits) - bchaves::core::BigInt(1);
    } else {
        size_t colon = options.range.find(':');
        if (colon == std::string::npos) {
            std::cerr << "[E] Formato de range invalido. Use -b bits ou -r start:end (HEX)\n";
            return 1;
        }
        bchaves::core::parse_big_int(options.range.substr(0, colon).c_str(), range_start);
        bchaves::core::parse_big_int(options.range.substr(colon + 1).c_str(), range_end);
    }

    init_jump_table();
    
    // Alvo Y (Ponto Secp256k1)
    AddressMatcher matcher_placeholder;
    auto target_load = bchaves::system::load_targets(options.target_path, true); // True exige pubkeys
    if (target_load.entries.empty()) {
        std::cerr << "[E] Nenhuma Public Key encontrada no arquivo de alvos.\n";
        return 1;
    }
    bchaves::core::Secp256k1Point target_y = bchaves::core::deserialize_pubkey(target_load.entries[0].payload.data(), target_load.entries[0].payload.size());
    if (target_y.infinity) {
        std::cerr << "[E] Falha ao desserializar Ponto Y.\n";
        return 1;
    }
    std::cout << "[+] Alvo Y carregado com sucesso.\n";

    std::vector<TrapShard> shards(16);
    std::atomic<uint64_t> total_hops{0};
    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::mutex sol_mtx;

    auto worker = [&](int thread_id) {
        (void)thread_id;
        // Fleet de 64 Kangaroos por Core (Etapa 2 da Arquitetura)
        struct KangarooMod {
            bchaves::core::PointJacobian p_jac;
            bchaves::core::Secp256k1Point p_aff;
            bchaves::core::BigInt distance;
            bool is_wild;
        };
        std::array<KangarooMod, 64> fleet;
        
        for(int i=0; i<64; ++i) {
            fleet[i].is_wild = (i % 2 == 0);
            bchaves::core::Secp256k1Point start_p = fleet[i].is_wild ? target_y : bchaves::core::secp256k1_multiply(range_end);
            fleet[i].distance = fleet[i].is_wild ? 0 : range_end;
            
            bchaves::core::BigInt offset(i * 1000);
            start_p = bchaves::core::secp256k1_add(start_p, bchaves::core::secp256k1_multiply(offset));
            fleet[i].distance = fleet[i].distance + offset; 
            
            fleet[i].p_jac = bchaves::core::to_jacobian(start_p.x, start_p.y);
            fleet[i].p_aff = start_p;
        }

        bchaves::core::PointJacobian batch_j[64];
        bchaves::core::Secp256k1Point batch_a[64];

        while(!g_stop_requested && !found.load()) {
            // Rodada de saltos para toda a frota
            for(int i=0; i<64; ++i) {
                uint32_t jump_idx = fleet[i].p_aff.x.limbs[0] % 64;
                fleet[i].p_jac = bchaves::core::add_points_mixed(fleet[i].p_jac, g_jump_table[jump_idx].point);
                fleet[i].distance = fleet[i].distance + g_jump_table[jump_idx].distance;
                batch_j[i] = fleet[i].p_jac;
            }

            // Normalização em massa da frota (1 mod_inv total)
            bchaves::core::batch_normalize(batch_j, batch_a, 64);
            total_hops += 64;

            for(int i=0; i<64; ++i) {
                fleet[i].p_aff = batch_a[i];
                auto& k = fleet[i];

                if (is_distinguished(k.p_aff.x, 16)) {
                    uint64_t h = k.p_aff.x.limbs[0];
                    int shard_idx = h % 16;
                    auto& shard = shards[shard_idx];
                    
                    std::lock_guard<std::mutex> lock(shard.mtx);
                    if (shard.table.count(h)) {
                        auto& other = shard.table[h];
                        if (other.is_wild != k.is_wild) {
                            std::lock_guard<std::mutex> slock(sol_mtx);
                            if (!found.load()) {
                                found = true;
                                if (k.is_wild) {
                                    solution = range_end + other.distance - k.distance;
                                } else {
                                    solution = range_end + k.distance - other.distance;
                                }
                            }
                            return;
                        }
                    } else {
                        shard.table[h] = {k.distance, k.is_wild};
                    }
                }
            }
        }
    };

    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    std::uint32_t num_threads = tune.threads;
    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    std::vector<std::thread> threads;
    for(std::uint32_t i=0; i<num_threads; ++i) threads.emplace_back(worker, i);

    auto hw = bchaves::system::detect_hardware();
    uint64_t max_traps = (hw.ram_available * 8) / 10 / 80; // 80% da RAM, ~80 bytes por trap
    if (max_traps < 100000) max_traps = 100000; // Mínimo de segurança
    
    std::cout << "[+] Limite de RAM: " << (hw.ram_available / 1024 / 1024) << " MB\n";
    std::cout << "[+] Limite de Armadilhas em RAM: " << max_traps << "\n";

    auto dump_shards = [&]() {
        std::cout << "\n[!] RAM em " << (hw.ram_available / 1024 / 1024) << " MB. Iniciando Dump de Armadilhas para disco...\n";
        std::error_code ec;
        std::filesystem::create_directories("traps", ec);
        
        for (int i = 0; i < 16; ++i) {
            auto& shard = shards[i];
            std::lock_guard<std::mutex> lock(shard.mtx);
            if (shard.table.empty()) continue;
            
            std::string filename = "traps/shard_" + std::to_string(i) + ".bin";
            std::ofstream out(filename, std::ios::binary | std::ios::app);
            if (out) {
                for (const auto& [hash, trap] : shard.table) {
                    out.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
                    out.write(reinterpret_cast<const char*>(trap.distance.limbs.data()), 32);
                    uint8_t wild = trap.is_wild ? 1 : 0;
                    out.write(reinterpret_cast<const char*>(&wild), 1);
                }
                shard.table.clear();
            }
        }
        std::cout << "[+] Dump concluído. Memória liberada.\n";
    };

    auto start_time = std::chrono::steady_clock::now();
    auto last_dump = start_time;

    while(!found.load() && !g_stop_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        uint64_t current_traps = 0;
        for(int i=0; i<16; ++i) {
            std::lock_guard<std::mutex> lock(shards[i].mtx);
            current_traps += shards[i].table.size();
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        double rate = static_cast<double>(total_hops.load()) / std::max<double>(1.0, (double)elapsed);
        
        std::cout << "\r[*] Total Hops: " << total_hops.load() 
                  << " | Speed: " << bchaves::system::format_rate(rate) 
                  << " | Traps in RAM: " << current_traps << " / " << max_traps << "        " << std::flush;

        // Dump se atingir o limite de RAM
        if (current_traps >= max_traps) {
            dump_shards();
            last_dump = now;
        }
    }

    for(auto& t : threads) if(t.joinable()) t.join();
    std::cout << "\n";

    if (found.load()) {
        bchaves::core::DerivedKeyInfo info;
        if (bchaves::core::derive_key_info(solution, info)) {
            bchaves::engine::report_found(info, "Kangaroo Search (range:" + options.range + ")");
        }
    }

    return 0;
}

}  // namespace bchaves::engine