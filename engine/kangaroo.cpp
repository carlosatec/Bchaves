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
    // Check if last N bits are zero
    std::uint32_t mask = (1u << bits) - 1;
    return (x.limbs[0] & mask) == 0;
}

} // namespace

int run_kangaroo(const bchaves::system::KangarooOptions& options) {
    std::signal(SIGINT, handle_sig);
    std::cout << "[+] Iniciando Kangaroo (Architectural Fleet Model)\n";
    
    // Parsing range "start:end"
    bchaves::core::BigInt range_start, range_end;
    size_t colon = options.range.find(':');
    if (colon == std::string::npos) {
        std::cerr << "[E] Formato de range invalido. Use -r start:end (HEX)\n";
        return 1;
    }
    bchaves::core::parse_big_int(options.range.substr(0, colon).c_str(), range_start);
    bchaves::core::parse_big_int(options.range.substr(colon + 1).c_str(), range_end);

    init_jump_table();
    
    // Alvo Y (Ponto Secp256k1)
    AddressMatcher matcher_placeholder;
    auto target_load = bchaves::system::load_targets(options.target_path, true); // True exige pubkeys
    if (target_load.entries.empty()) {
        std::cerr << "[E] Nenhuma Public Key encontrada no arquivo de alvos.\n";
        return 1;
    }
    bchaves::core::Secp256k1Point target_y = bchaves::core::deserialize_pubkey(target_load.entries[0].payload);
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
        // Fleet de 64 Kangaroos por Core (Etapa 2 da Arquitetura)
        std::array<Kangaroo, 64> fleet;
        for(int i=0; i<64; ++i) {
            fleet[i].is_wild = (i % 2 == 0);
            if (fleet[i].is_wild) {
                fleet[i].point = target_y; // Wild partos de Y
                fleet[i].distance = 0;
            } else {
                fleet[i].point = bchaves::core::secp256k1_multiply(range_end); // Tame parte do fim
                fleet[i].distance = range_end;
            }
            // Adicionar offset aleatório para cada canguru da frota
            bchaves::core::BigInt offset(i * 1000);
            fleet[i].point = bchaves::core::secp256k1_add(fleet[i].point, bchaves::core::secp256k1_multiply(offset));
            fleet[i].distance = fleet[i].distance + (fleet[i].is_wild ? offset : offset); 
        }

        while(!g_stop_requested && !found.load()) {
            for(int i=0; i<64; ++i) {
                auto& k = fleet[i];
                uint32_t jump_idx = k.point.x.limbs[0] % 64;
                k.point = bchaves::core::secp256k1_add(k.point, g_jump_table[jump_idx].point);
                k.distance = k.distance + g_jump_table[jump_idx].distance;
                total_hops++;

                if (is_distinguished(k.point.x, 16)) {
                    uint64_t h = k.point.x.limbs[0];
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
                                    // Wild (Y + d_w*G) hit Tame (end*G + d_t*G)
                                    // Y = (end + d_t - d_w) * G
                                    solution = range_end + other.distance - k.distance;
                                } else {
                                    // Tame hit Wild
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

    std::vector<std::thread> threads;
    std::uint32_t num_threads = options.threads > 0 ? options.threads : 4;
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
        std::cout << "[!!!] CHAVE ENCONTRADA: " << bchaves::core::bigint_to_hex(solution) << "\n";
        std::ofstream f("FOUND.txt", std::ios::app);
        f << "Kangaroo Found: " << bchaves::core::bigint_to_hex(solution) << "\n";
    }

    return 0;
}

}  // namespace bchaves::engine