/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Algoritmo Baby-Step Giant-Step (BSGS) com Cuckoo Filter.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "core/secp256k1.hpp"
#include "core/cuckoo.hpp"
#include "core/hash.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <fstream>
#include <unordered_map>

namespace bchaves::engine {
namespace {

struct Entry {
    uint64_t hash;
    uint64_t index;
    bool operator<(const Entry& other) const { return hash < other.hash; }
};

struct BSGSShard {
    std::mutex mtx; // Necessário apenas durante a geração
    std::vector<Entry> table;
};

} // namespace

int run_bsgs(const bchaves::system::BsgsOptions& options) {
    auto hardware = bchaves::system::detect_hardware();
    if (options.help) {
        std::cout << "[*] Hardware Detectado:\n"
                  << "    Cores: " << hardware.num_cores << " (Fisicos: " << hardware.num_physical_cores << ")\n"
                  << "    RAM: " << (hardware.ram_total / (1024*1024*1024)) << " GB (Livre: " << (hardware.ram_available / (1024*1024*1024)) << " GB)\n"
                  << "    Cache L3: " << (hardware.l3_cache / (1024*1024)) << " MB\n"
                  << "    Features: " << (hardware.features & bchaves::system::cpu_avx2 ? "AVX2 " : "")
                                     << (hardware.features & bchaves::system::cpu_bmi2 ? "BMI2 " : "") 
                                     << (hardware.features & bchaves::system::cpu_sha_ni ? "SHA-NI " : "") << "\n";
        return 0;
    }

    std::cout << "[+] Iniciando BSGS (Cuckoo Filter Accelerated)\n";
    
    // Resolve range
    uint32_t bits = options.bits;
    bchaves::core::BigInt total_range(0);
    total_range.limbs[bits/32] = (1u << (bits%32));
    
    uint32_t b_bits = bits / 2;
    uint64_t num_baby_steps = 1ULL << b_bits;
    std::cout << "[+] Baby Steps: 2^" << b_bits << " (" << num_baby_steps << ")\n";

    // Alvo Y (Ponto Secp256k1)
    auto target_load = bchaves::system::load_targets(options.target_path, true); 
    if (target_load.entries.empty()) {
        std::cerr << "[E] Nenhuma Public Key encontrada para o BSGS.\n";
        return 1;
    }
    bchaves::core::Secp256k1Point target_y = bchaves::core::deserialize_pubkey(target_load.entries[0].payload.data(), target_load.entries[0].payload.size());
    if (target_y.infinity) {
        std::cerr << "[E] Target chave invalida (infinity point)\n";
        return 1;
    }

    // FASE 1: Baby Steps
    bchaves::core::CuckooFilter filter(num_baby_steps);
    std::vector<BSGSShard> shards(16);
    
    std::cout << "[*] Gerando Tabela de Baby Steps...\n";
    bchaves::core::Secp256k1Point g = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(1));
    bchaves::core::PointJacobian current_p_jac = bchaves::core::to_jacobian(g.x, g.y);
    
    static constexpr size_t kBabyBatch = 1024;
    std::vector<bchaves::core::PointJacobian> batch_p(kBabyBatch);
    std::vector<bchaves::core::Secp256k1Point> batch_affine(kBabyBatch);

    for (uint64_t i = 1; i <= num_baby_steps; i += kBabyBatch) {
        size_t current_batch_size = std::min<uint64_t>(kBabyBatch, num_baby_steps - i + 1);
        
        for (size_t k = 0; k < current_batch_size; ++k) {
            batch_p[k] = current_p_jac;
            current_p_jac = bchaves::core::add_points_mixed(current_p_jac, g);
        }
        
        bchaves::core::batch_normalize(batch_p.data(), batch_affine.data(), current_batch_size);
        
        for (size_t k = 0; k < current_batch_size; ++k) {
            uint64_t h = batch_affine[k].x.limbs[0];
            filter.insert(h);
            int s = h % 16;
            shards[s].table.push_back({h, i + k});
        }

        if (i % 1000000 == 0) std::cout << "\r    " << (i/1000000) << "M pontos..." << std::flush;
    }
    std::cout << "\n[*] Ordenando tabela (Fase Final)..." << std::flush;
    
    std::vector<std::thread> sort_threads;
    for(int s=0; s<16; ++s) {
        sort_threads.emplace_back([&shards, s]() {
            std::sort(shards[s].table.begin(), shards[s].table.end());
        });
    }
    for(auto& t : sort_threads) t.join();
    
    std::cout << "\n[+] Tabela concluída.\n";

    // FASE 2: Giant Steps
    bchaves::core::BigInt step_size(num_baby_steps);
    bchaves::core::Secp256k1Point giant_step_point = bchaves::core::secp256k1_multiply(step_size);
    bchaves::core::Secp256k1Point neg_step = giant_step_point;
    neg_step.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), neg_step.y, bchaves::core::kFieldPrime);

    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::atomic<uint64_t> giant_count{0};

    auto worker = [&](int tid, int num_threads) {
        bchaves::core::Secp256k1Point current_giant_affine = target_y;
        if (tid > 0) {
            bchaves::core::BigInt skip = step_size;
            bchaves::core::mul_small_in_place(skip, tid);
            bchaves::core::Secp256k1Point skip_p = bchaves::core::secp256k1_multiply(skip);
            skip_p.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), skip_p.y, bchaves::core::kFieldPrime);
            current_giant_affine = bchaves::core::secp256k1_add(current_giant_affine, skip_p);
        }
        
        bchaves::core::PointJacobian current_giant_jac = bchaves::core::to_jacobian(current_giant_affine.x, current_giant_affine.y);
        
        bchaves::core::BigInt big_jump_val = step_size;
        bchaves::core::mul_small_in_place(big_jump_val, num_threads);
        bchaves::core::Secp256k1Point big_jump = bchaves::core::secp256k1_multiply(big_jump_val);
        big_jump.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), big_jump.y, bchaves::core::kFieldPrime);

        static constexpr size_t kGiantBatch = 256;
        bchaves::core::PointJacobian batch_gj[kGiantBatch];
        bchaves::core::Secp256k1Point batch_ga[kGiantBatch];
        uint64_t batch_j[kGiantBatch];

        uint64_t j = tid;
        while (!found.load()) {
            for (size_t k = 0; k < kGiantBatch; ++k) {
                batch_gj[k] = current_giant_jac;
                batch_j[k] = j;
                current_giant_jac = bchaves::core::add_points_mixed(current_giant_jac, big_jump);
                j += num_threads;
            }

            bchaves::core::batch_normalize(batch_gj, batch_ga, kGiantBatch);

            for (size_t k = 0; k < kGiantBatch; ++k) {
                uint64_t h = batch_ga[k].x.limbs[0];
                if (filter.lookup(h)) {
                    int s = h % 16;
                    auto& st = shards[s].table;
                    auto it = std::lower_bound(st.begin(), st.end(), Entry{h, 0});
                    if (it != st.end() && it->hash == h) {
                        bchaves::core::BigInt i_val(it->index);
                        bchaves::core::BigInt j_val(batch_j[k]);
                        solution = (j_val * step_size) + i_val;
                        found = true;
                        return;
                    }
                }
            }
            giant_count += kGiantBatch;
        }
    };

    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    std::uint32_t num_threads = tune.threads;
    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    std::vector<std::thread> threads;
    for(uint32_t i=0; i<num_threads; ++i) threads.emplace_back(worker, i, num_threads);

    while (!found.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "\r[*] Giant Steps: " << giant_count.load() << std::flush;
    }

    for (auto& t : threads) t.join();

    if (found.load()) {
        std::cout << "\n[!!!] CHAVE ENCONTRADA (BSGS): " << bchaves::core::bigint_to_hex(solution) << "\n";
        std::ofstream f("FOUND.txt", std::ios::app);
        f << "BSGS Found: " << bchaves::core::bigint_to_hex(solution) << "\n";
    }

    return 0;
}

}  // namespace bchaves::engine