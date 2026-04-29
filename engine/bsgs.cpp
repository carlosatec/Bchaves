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

#include <algorithm>
#include <ctime>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace bchaves::engine {

namespace {

bool ceil_div_bigint_u64_to_u64(const bchaves::core::BigInt& num,
                                std::uint64_t denom,
                                std::uint64_t& out) {
    if (denom == 0) {
        return false;
    }
    __uint128_t rem = 0;
    std::uint64_t quotient[4] = {};
    for (int i = 3; i >= 0; --i) {
        const __uint128_t cur = (rem << 64) | num.limbs[i];
        quotient[i] = static_cast<std::uint64_t>(cur / denom);
        rem = cur % denom;
    }
    if (quotient[1] != 0 || quotient[2] != 0 || quotient[3] != 0) {
        return false;
    }
    out = quotient[0];
    if (rem != 0) {
        if (out == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        ++out;
    }
    return true;
}

}  // namespace


struct Entry {
    bchaves::core::BigInt x;  // full 256-bit x-coordinate
    bool odd;
    uint64_t index;
    bool operator<(const Entry& other) const {
        if (x != other.x) return x < other.x;
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

    std::string backend_error;
    if (!configure_secp256k1_backend(options.secp256k1_backend, backend_error)) {
        std::cerr << "[E] Falha ao configurar secp256k1: " << backend_error << '\n';
        return 1;
    }
    
    uint32_t bits = options.bits;
    uint64_t num_baby_steps = 0;
    if (options.table_k > 0) {
        if (options.table_k > (std::numeric_limits<std::uint64_t>::max() / 1024ULL)) {
            std::cerr << "[E] Valor de -k excede o limite suportado.\n";
            return 1;
        }
        num_baby_steps = 1024ULL * options.table_k;
    } else {
        const uint32_t b_bits = bits / 2;
        if (b_bits >= 63) {
            std::cerr << "[E] BSGS atual suporta no maximo 126 bits por limite de indexacao interna.\n";
            return 1;
        }
        num_baby_steps = 1ULL << b_bits;
    }
    if (num_baby_steps == 0) {
        std::cerr << "[E] Numero de baby steps invalido.\n";
        return 1;
    }
    if (bits > 126) {
        std::cerr << "[E] BSGS atual suporta no maximo 126 bits por limite de indexacao interna.\n";
        return 1;
    }
    std::cout << "[+] Baby Steps: " << num_baby_steps;
    if (options.table_k > 0) {
        std::cout << " (-k " << options.table_k << ")";
    }
    std::cout << "\n";

    bchaves::core::BigInt range_start;
    if (bits == 1) {
        range_start = bchaves::core::BigInt(1);
    } else {
        range_start = bchaves::core::BigInt(1) << (bits - 1);
    }
    const bchaves::core::BigInt range_end = (bchaves::core::BigInt(1) << bits) - bchaves::core::BigInt(1);
    std::uint64_t max_giant_steps = 0;
    if (!ceil_div_bigint_u64_to_u64(range_end, num_baby_steps, max_giant_steps)) {
        std::cerr << "[E] O range atual exige mais de 2^64 giant steps. Aumente -k ou reduza -b.\n";
        return 1;
    }

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
            shards[s].table.push_back({batch_affine[k].x, batch_affine[k].y.is_odd(), i + k});
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

    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::atomic<uint64_t> giant_count{0};
    std::atomic<std::uint32_t> active_workers{0};

    bchaves::system::CheckpointState checkpoint;
    checkpoint.algorithm = "bsgs";
    checkpoint.progress_secondary = options.bits;
    checkpoint.current = bchaves::core::to_bytes32(step_size);
    auto worker = [&](int tid, int num_threads) {
        struct WorkerExitGuard {
            std::atomic<std::uint32_t>& counter;
            ~WorkerExitGuard() { counter.fetch_sub(1, std::memory_order_relaxed); }
        } guard{active_workers};
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
        while (!found.load(std::memory_order_relaxed)) {
            std::size_t batch_count = 0;
            for (; batch_count < kGiantBatch && j < max_giant_steps; ++batch_count) {
                batch_gj[batch_count] = current_giant_jac;
                batch_j[batch_count] = j;
                current_giant_jac = bchaves::core::add_points_mixed(current_giant_jac, big_jump);
                j += num_threads;
            }
            if (batch_count == 0) {
                return;
            }

            bchaves::core::batch_normalize(batch_gj, batch_ga, batch_count);

            for (size_t k = 0; k < batch_count; ++k) {
                uint64_t h = batch_ga[k].x.limbs[0];
                if (filter.lookup(h)) {
                    int s = h % 16;
                    auto& st = shards[s].table;
                    Entry probe{batch_ga[k].x, batch_ga[k].y.is_odd(), 0};
                    auto it = std::lower_bound(st.begin(), st.end(), probe);
                    while (it != st.end() && it->x == batch_ga[k].x && it->odd == batch_ga[k].y.is_odd()) {
                        bchaves::core::BigInt i_val(it->index);
                        bchaves::core::BigInt j_val(batch_j[k]);
                        bchaves::core::BigInt candidate = (j_val * step_size) + i_val;
                        if (candidate >= range_start && candidate <= range_end && matches_target(candidate, target_y)) {
                            solution = candidate;
                            found = true;
                            return;
                        }
                        ++it;
                    }
                }
            }
            giant_count += batch_count;
        }
    };


    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads, options.table_k);
    std::uint32_t num_threads = tune.threads;

    const std::string checkpoint_name = "bsgs_" + std::to_string(options.bits) + "bit.ckp";
    const std::filesystem::path checkpoint_path = options.checkpoint_path.value_or(std::filesystem::path(checkpoint_name));
    
    if (options.checkpoint_enabled && std::filesystem::exists(checkpoint_path)) {
        std::string err;
        if (bchaves::system::load_checkpoint(checkpoint_path, checkpoint, err)) {
            if (checkpoint.algorithm == "bsgs"
                && checkpoint.progress_secondary == options.bits
                && checkpoint.current == bchaves::core::to_bytes32(step_size)) {
                std::cout << "[+] Checkpoint detectado. Retomando de Giant Step: " << checkpoint.progress_primary << "\n";
            } else {
                std::cerr << "[!] Checkpoint incompativel. Ignorando.\n";
                checkpoint.progress_primary = 0;
            }
        }
    }

    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < num_threads; ++i) {
        active_workers.fetch_add(1, std::memory_order_relaxed);
        threads.emplace_back(worker, i, num_threads);
    }

    auto last_checkpoint = std::chrono::steady_clock::now();
    while (!found.load() && active_workers.load(std::memory_order_relaxed) > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto now = std::chrono::steady_clock::now();
        std::cout << "\r[*] Giant Steps: " << giant_count.load() + checkpoint.progress_primary << std::flush;

        if (options.checkpoint_enabled && !options.benchmark && 
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            
            bchaves::system::CheckpointState ckp;
            ckp.algorithm = "bsgs";
            ckp.progress_secondary = options.bits;
            ckp.current = bchaves::core::to_bytes32(step_size);
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
    } else {
        std::cout << "[!] Exhausted: alvo nao encontrado no range solicitado.\n";
    }

    return 0;
}

}  // namespace bchaves::engine
