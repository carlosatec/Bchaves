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
#include "system/checkpoint.hpp"
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


struct Entry {
    uint64_t hash;
    bool odd;
    uint64_t index;
    bool operator<(const Entry& other) const {
        if (hash != other.hash) return hash < other.hash;
        if (odd != other.odd) return odd < other.odd;
        return index < other.index;
    }
};

struct BSGSShard {
    std::mutex mtx; // Necessário apenas durante a geração
    std::vector<Entry> table;
};

bool matches_target(const bchaves::core::BigInt& candidate, const bchaves::core::Secp256k1Point& target) {
    const bchaves::core::Secp256k1Point pub = bchaves::core::secp256k1_multiply(candidate);
    return !pub.infinity && pub.x == target.x && pub.y == target.y;
}



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
    
    uint32_t bits = options.bits;
    uint32_t b_bits = bits / 2;
    if (b_bits >= 63) {
        std::cerr << "[E] BSGS atual suporta no maximo 126 bits por limite de indexacao interna.\n";
        return 1;
    }
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
            shards[s].table.push_back({h, batch_affine[k].y.is_odd(), i + k});
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

    bchaves::system::CheckpointState checkpoint;
    auto worker = [&](int tid, int num_threads) {
        bchaves::system::pin_thread_to_core(static_cast<std::uint32_t>(tid));
        bchaves::core::BigInt giant_step_idx(tid);
        if (checkpoint.progress_primary > 0) {
            giant_step_idx = giant_step_idx + bchaves::core::BigInt(checkpoint.progress_primary);
        }

        bchaves::core::Secp256k1Point current_giant_affine = target_y;
        if (giant_step_idx > 0) {
            bchaves::core::Secp256k1Point skip_p = bchaves::core::secp256k1_multiply(giant_step_idx * step_size);
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

        uint64_t j = tid + checkpoint.progress_primary;
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
                    auto it = std::lower_bound(st.begin(), st.end(), Entry{h, batch_ga[k].y.is_odd(), 0});
                    while (it != st.end() && it->hash == h && it->odd == batch_ga[k].y.is_odd()) {
                        bchaves::core::BigInt i_val(it->index);
                        bchaves::core::BigInt j_val(batch_j[k]);
                        bchaves::core::BigInt candidate = (j_val * step_size) + i_val;
                        if (matches_target(candidate, target_y)) {
                            solution = candidate;
                            found = true;
                            return;
                        }
                        ++it;
                    }
                }
            }
            giant_count += kGiantBatch;
        }
    };


    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    std::uint32_t num_threads = tune.threads;

    const std::string checkpoint_name = "bsgs_" + std::to_string(options.bits) + "bit.ckp";
    const std::filesystem::path checkpoint_path = options.checkpoint_path.value_or(std::filesystem::path(checkpoint_name));
    
    if (options.checkpoint_enabled && std::filesystem::exists(checkpoint_path)) {
        std::string err;
        if (bchaves::system::load_checkpoint(checkpoint_path, checkpoint, err)) {
            if (checkpoint.algorithm == "bsgs" && checkpoint.progress_secondary == options.bits) {
                std::cout << "[+] Checkpoint detectado. Retomando de Giant Step: " << checkpoint.progress_primary << "\n";
            } else {
                std::cerr << "[!] Checkpoint incompativel. Ignorando.\n";
                checkpoint.progress_primary = 0;
            }
        }
    }

    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    std::vector<std::thread> threads;
    for(uint32_t i=0; i<num_threads; ++i) threads.emplace_back(worker, i, num_threads);

    auto last_checkpoint = std::chrono::steady_clock::now();
    while (!found.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto now = std::chrono::steady_clock::now();
        std::cout << "\r[*] Giant Steps: " << giant_count.load() + checkpoint.progress_primary << std::flush;

        if (options.checkpoint_enabled && !options.benchmark && 
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            
            bchaves::system::CheckpointState ckp;
            ckp.algorithm = "bsgs";
            ckp.progress_secondary = options.bits;
            ckp.progress_primary = giant_count.load() + checkpoint.progress_primary;
            ckp.timestamp = static_cast<uint64_t>(std::time(nullptr));
            
            std::string err;
            if (bchaves::system::save_checkpoint(checkpoint_path, ckp, err)) {
                // Silencioso
            }
            last_checkpoint = now;
        }
    }

    for (auto& t : threads) t.join();

    if (found.load()) {
        bchaves::core::DerivedKeyInfo info;
        if (bchaves::core::derive_key_info(solution, info)) {
            bchaves::engine::report_found(
                info,
                "BSGS Search (bits:" + std::to_string(options.bits) + ")",
                !options.benchmark);
        }
    }

    return 0;
}

}  // namespace bchaves::engine
