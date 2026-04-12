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

struct BabyStep {
    bchaves::core::BigInt index;
};

struct BSGSShard {
    std::mutex mtx;
    std::unordered_map<uint64_t, bchaves::core::BigInt> table;
};

} // namespace

int run_bsgs(const bchaves::system::BsgsOptions& options) {
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
    bchaves::core::Secp256k1Point target_y = bchaves::core::deserialize_pubkey(target_load.entries[0].payload);
    if (target_y.infinity) return 1;

    // FASE 1: Baby Steps
    bchaves::core::CuckooFilter filter(num_baby_steps);
    std::vector<BSGSShard> shards(16);
    
    std::cout << "[*] Gerando Tabela de Baby Steps...\n";
    bchaves::core::Secp256k1Point current_p = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(0)); // Infinity? No, start at 1
    bchaves::core::Secp256k1Point g = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(1));
    current_p = g; 

    for (uint64_t i = 1; i <= num_baby_steps; ++i) {
        uint64_t h = current_p.x.limbs[0];
        filter.insert(h);
        int s = h % 16;
        shards[s].table[h] = bchaves::core::BigInt(i);
        current_p = bchaves::core::secp256k1_add(current_p, g);
        if (i % 1000000 == 0) std::cout << "\r    " << (i/1000000) << "M pontos..." << std::flush;
    }
    std::cout << "\n[+] Tabela concluída.\n";

    // FASE 2: Giant Steps
    bchaves::core::BigInt step_size(num_baby_steps);
    bchaves::core::Secp256k1Point giant_step_point = bchaves::core::secp256k1_multiply(step_size);
    // Para retroceder Y - j*B*G, usamos Y + j*(-B*G)
    // Negativo de (x, y) é (x, -y)
    bchaves::core::Secp256k1Point neg_step = giant_step_point;
    neg_step.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), neg_step.y, bchaves::core::kFieldPrime);

    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::atomic<uint64_t> giant_count{0};

    auto worker = [&](int tid, int num_threads) {
        bchaves::core::Secp256k1Point current_giant = target_y;
        // Ponto inicial da thread: Y - tid * (B*G)
        if (tid > 0) {
            bchaves::core::BigInt skip = step_size;
            bchaves::core::mul_small_in_place(skip, tid);
            bchaves::core::Secp256k1Point skip_p = bchaves::core::secp256k1_multiply(skip);
            skip_p.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), skip_p.y, bchaves::core::kFieldPrime);
            current_giant = bchaves::core::secp256k1_add(current_giant, skip_p);
        }
        
        // Pulo da thread: -num_threads * B * G
        bchaves::core::BigInt big_jump_val = step_size;
        bchaves::core::mul_small_in_place(big_jump_val, num_threads);
        bchaves::core::Secp256k1Point big_jump = bchaves::core::secp256k1_multiply(big_jump_val);
        big_jump.y = bchaves::core::mod_sub(bchaves::core::BigInt(0), big_jump.y, bchaves::core::kFieldPrime);

        uint64_t j = tid;
        while (!found.load()) {
            uint64_t h = current_giant.x.limbs[0];
            if (filter.lookup(h)) {
                int s = h % 16;
                std::lock_guard<std::mutex> lock(shards[s].mtx);
                if (shards[s].table.count(h)) {
                    bchaves::core::BigInt i_val = shards[s].table[h];
                    // Y - j*B*G = i*G  => Y = (j*B + i)*G
                    bchaves::core::BigInt j_val(j);
                    solution = (j_val * step_size) + i_val;
                    found = true;
                    return;
                }
            }
            current_giant = bchaves::core::secp256k1_add(current_giant, big_jump);
            j += num_threads;
            giant_count++;
        }
    };

    std::uint32_t num_threads = options.threads > 0 ? options.threads : 4;
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