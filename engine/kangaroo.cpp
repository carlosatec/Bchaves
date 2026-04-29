/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Algoritmo Pollard's Kangaroo (Architectural Fleet Model).
 *            Ultra-RAM Edition: Persistência de armadilhas, Cuckoo Filter,
 *            e escrita bufferizada para disco lento.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"
#include "core/secp256k1.hpp"
#include "core/hash.hpp"
#include "core/cuckoo.hpp"
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <csignal>
#include <fstream>
#include <cstring>
#include <chrono>

namespace bchaves::engine {
namespace {

std::atomic<bool> g_stop_requested{false};
void handle_sig(int) { g_stop_requested = true; }

// ============================================================
// Estruturas de Dados
// ============================================================

// Estrutura compacta de armadilha (41 bytes por entrada no disco)
// Em RAM: 40 bytes via unordered_map overhead
struct KangarooTrap {
    bchaves::core::BigInt distance;
    bool is_wild;
};

struct TrapShard {
    std::mutex mtx;
    std::unordered_map<std::uint64_t, KangarooTrap> table;
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

// ============================================================
// Constantes do Formato de Arquivo de Armadilhas
// ============================================================
static constexpr uint32_t TRAP_MAGIC    = 0x42544B47; // "BTKG" (Bchaves Trap Kangaroo)
static constexpr uint32_t TRAP_VERSION  = 2;
static constexpr size_t   TRAP_HEADER_SIZE = 4 + 4 + 32 + 32; // magic + version + range_start + range_end

// ============================================================
// Jump Table Global - Alinhada em cache line (64 bytes)
// ============================================================
alignas(64) std::array<Jump, 64> g_jump_table;

void init_jump_table() {
    bchaves::core::BigInt d(1);
    for(int i=0; i<64; ++i) {
        g_jump_table[i].distance = d;
        g_jump_table[i].point = bchaves::core::secp256k1_multiply(d);
        d = d << 1;
    }
}

// Inline forçado para eliminar overhead de chamada no hot loop
__attribute__((always_inline))
inline bool is_distinguished(const bchaves::core::BigInt& x, std::uint32_t bits) {
    if (__builtin_expect(bits == 0, 0)) return true;
    if (__builtin_expect(bits >= 64, 0)) return x.limbs[0] == 0;
    uint64_t mask = (1ULL << bits) - 1;
    return __builtin_expect((x.limbs[0] & mask) == 0, 0);
}

// ============================================================
// Fase 1: Persistência - Escrita com Header Validado
// ============================================================

bool write_trap_header(std::ofstream& out,
                       const bchaves::core::BigInt& range_start,
                       const bchaves::core::BigInt& range_end) {
    out.write(reinterpret_cast<const char*>(&TRAP_MAGIC), 4);
    out.write(reinterpret_cast<const char*>(&TRAP_VERSION), 4);
    out.write(reinterpret_cast<const char*>(range_start.limbs.data()), 32);
    out.write(reinterpret_cast<const char*>(range_end.limbs.data()), 32);
    return out.good();
}

bool validate_trap_header(std::ifstream& in,
                          const bchaves::core::BigInt& range_start,
                          const bchaves::core::BigInt& range_end) {
    uint32_t magic = 0, version = 0;
    bchaves::core::BigInt file_start, file_end;

    in.read(reinterpret_cast<char*>(&magic), 4);
    in.read(reinterpret_cast<char*>(&version), 4);
    in.read(reinterpret_cast<char*>(file_start.limbs.data()), 32);
    in.read(reinterpret_cast<char*>(file_end.limbs.data()), 32);

    if (!in.good()) return false;
    if (magic != TRAP_MAGIC) return false;
    if (version != TRAP_VERSION) return false;

    // Verificar se o range é o mesmo
    for (int i = 0; i < 4; ++i) {
        if (file_start.limbs[i] != range_start.limbs[i]) return false;
        if (file_end.limbs[i] != range_end.limbs[i]) return false;
    }
    return true;
}

// ============================================================
// Fase 1: Cold Boot - Carregar armadilhas do disco para RAM
// ============================================================

uint64_t load_traps_from_disk(std::vector<TrapShard>& shards,
                              bchaves::core::CuckooFilter& filter,
                              const bchaves::core::BigInt& range_start,
                              const bchaves::core::BigInt& range_end) {
    uint64_t loaded = 0;
    std::error_code ec;

    if (!std::filesystem::exists("traps", ec)) return 0;

    auto start_time = std::chrono::steady_clock::now();
    std::cout << "[+] Cold Boot: Carregando armadilhas do disco...\n";

    for (int i = 0; i < 16; ++i) {
        std::string filename = "traps/shard_" + std::to_string(i) + ".bin";
        std::ifstream in(filename, std::ios::binary);
        if (!in.is_open()) continue;

        // Validar header
        if (!validate_trap_header(in, range_start, range_end)) {
            std::cerr << "[!] Arquivo " << filename
                      << " tem range incompatível. Ignorando.\n";
            continue;
        }

        // Ler armadilhas
        while (in.good() && !in.eof()) {
            uint64_t hash = 0;
            bchaves::core::BigInt distance;
            uint8_t wild = 0;

            in.read(reinterpret_cast<char*>(&hash), sizeof(hash));
            in.read(reinterpret_cast<char*>(distance.limbs.data()), 32);
            in.read(reinterpret_cast<char*>(&wild), 1);

            if (!in.good()) break;

            int shard_idx = hash % 16;
            auto& shard = shards[shard_idx];
            std::lock_guard<std::mutex> lock(shard.mtx);
            shard.table[hash] = {distance, wild != 0};
            filter.insert(hash);
            ++loaded;
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    std::cout << "[+] Cold Boot concluído: " << loaded
              << " armadilhas carregadas em " << elapsed << "ms\n";
    return loaded;
}

// ============================================================
// Fase 1: Dump Bufferizado - Escrita assíncrona para disco
// ============================================================

// Buscar uma armadilha no arquivo de shard no disco.
// Retorna true se encontrou e preenche 'out_trap'.
bool lookup_trap_on_disk(int shard_idx,
                         uint64_t target_hash,
                         KangarooTrap& out_trap,
                         const bchaves::core::BigInt& range_start,
                         const bchaves::core::BigInt& range_end) {
    std::string filename = "traps/shard_" + std::to_string(shard_idx) + ".bin";
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) return false;
    if (!validate_trap_header(in, range_start, range_end)) return false;

    while (in.good() && !in.eof()) {
        uint64_t hash = 0;
        bchaves::core::BigInt distance;
        uint8_t wild = 0;
        in.read(reinterpret_cast<char*>(&hash), sizeof(hash));
        in.read(reinterpret_cast<char*>(distance.limbs.data()), 32);
        in.read(reinterpret_cast<char*>(&wild), 1);
        if (!in.good()) break;
        if (hash == target_hash) {
            out_trap = {distance, wild != 0};
            return true;
        }
    }
    return false;
}

void dump_shards_to_disk(std::vector<TrapShard>& shards,
                         const bchaves::core::BigInt& range_start,
                         const bchaves::core::BigInt& range_end) {
    std::error_code ec;
    std::filesystem::create_directories("traps", ec);

    uint64_t total_dumped = 0;

    for (int i = 0; i < 16; ++i) {
        auto& shard = shards[i];
        std::lock_guard<std::mutex> lock(shard.mtx);
        if (shard.table.empty()) continue;

        std::string filename = "traps/shard_" + std::to_string(i) + ".bin";

        // Merge: carregar traps existentes do disco, adicionar as da RAM, reescrever tudo.
        // Isto preserva o histórico completo sem duplicatas.
        std::unordered_map<uint64_t, KangarooTrap> merged;

        // 1. Carregar dados existentes do disco
        {
            std::ifstream in(filename, std::ios::binary);
            if (in.is_open() && validate_trap_header(in, range_start, range_end)) {
                while (in.good() && !in.eof()) {
                    uint64_t hash = 0;
                    bchaves::core::BigInt distance;
                    uint8_t wild = 0;
                    in.read(reinterpret_cast<char*>(&hash), sizeof(hash));
                    in.read(reinterpret_cast<char*>(distance.limbs.data()), 32);
                    in.read(reinterpret_cast<char*>(&wild), 1);
                    if (!in.good()) break;
                    merged[hash] = {distance, wild != 0};
                }
            }
        }

        // 2. Sobrescrever com dados da RAM (mais recentes)
        for (const auto& [hash, trap] : shard.table) {
            merged[hash] = trap;
        }

        // 3. Reescrever o arquivo completo com o merge
        std::ofstream out(filename, std::ios::binary | std::ios::trunc);
        if (!out) continue;

        write_trap_header(out, range_start, range_end);

        constexpr size_t ENTRY_SIZE = 8 + 32 + 1;
        std::vector<char> write_buf;
        write_buf.reserve(merged.size() * ENTRY_SIZE);

        for (const auto& [hash, trap] : merged) {
            const char* hp = reinterpret_cast<const char*>(&hash);
            write_buf.insert(write_buf.end(), hp, hp + sizeof(hash));

            const char* dp = reinterpret_cast<const char*>(trap.distance.limbs.data());
            write_buf.insert(write_buf.end(), dp, dp + 32);

            uint8_t wild = trap.is_wild ? 1 : 0;
            write_buf.push_back(static_cast<char>(wild));
        }

        out.write(write_buf.data(), static_cast<std::streamsize>(write_buf.size()));
        total_dumped += merged.size();
    }

    std::cout << "\n[+] Dump merge: " << total_dumped
              << " armadilhas totais no disco (RAM + histórico).\n";
}

} // namespace

int run_kangaroo(const bchaves::system::KangarooOptions& options) {
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

    std::signal(SIGINT, handle_sig);
    std::cout << "[+] Iniciando Kangaroo (Ultra-RAM Fleet Model)\n";
    
    bchaves::core::BigInt range_start, range_end;
    if (options.range.find("bits:") == 0) {
        uint32_t bits = std::stoul(options.range.substr(5));
        std::cout << "[*] Modo Bits Detectado: " << bits << "\n";
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
    auto target_load = bchaves::system::load_targets(options.target_path, true);
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

    // ============================================================
    // Configuração de Memória e Filtro
    // ============================================================
    auto hw = bchaves::system::detect_hardware();
    uint64_t max_traps = (hw.ram_available * 8) / 10 / 48; // 80% da RAM, ~48 bytes por trap
    if (max_traps < 100000) max_traps = 100000;

    // Cuckoo Filter dimensionado para o total esperado (RAM + Disco).
    // O filtro NUNCA é limpo, pois ele representa o universo completo de armadilhas
    // que existem em RAM + Disco combinados. ~128MB para 500M entradas.
    constexpr uint64_t FILTER_CAPACITY = 500000000ULL; // 500M entradas
    auto trap_filter = std::make_unique<bchaves::core::CuckooFilter>(FILTER_CAPACITY);

    std::cout << "[+] Limite de RAM: " << (hw.ram_available / 1024 / 1024) << " MB\n";
    std::cout << "[+] Limite de Armadilhas em RAM: " << max_traps << "\n";

    std::vector<TrapShard> shards(16);
    std::atomic<uint64_t> total_hops{0};
    std::atomic<uint64_t> total_traps_in_ram{0};
    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::mutex sol_mtx;

    // ============================================================
    // Fase 1: Cold Boot - Carregar armadilhas salvas anteriormente
    // ============================================================
    if (options.no_load) {
        std::cout << "[+] --no-load ativado: pulando carregamento de armadilhas do disco.\n";
    } else {
        uint64_t preloaded = load_traps_from_disk(shards, *trap_filter, range_start, range_end);
        total_traps_in_ram.store(preloaded);
    }

    // ============================================================
    // Worker: Fleet de 64 Kangaroos por Thread
    // ============================================================
    auto worker = [&](int thread_id) {
        (void)thread_id;
        // Fase 4: Estruturas alinhadas em cache line para SSE
        struct alignas(64) KangarooMod {
            bchaves::core::PointJacobian p_jac;
            bchaves::core::Secp256k1Point p_aff;
            bchaves::core::BigInt distance;
            bool is_wild;
        };
        std::array<KangarooMod, 64> fleet;
        
        // Wild/Tame ratio configurável via --wild / --tame
        const uint32_t wild_count = static_cast<uint32_t>((64 * options.wild_ratio) / 100);
        for(int i=0; i<64; ++i) {
            fleet[i].is_wild = (static_cast<uint32_t>(i) < wild_count);
            bchaves::core::Secp256k1Point start_p = fleet[i].is_wild ? target_y : bchaves::core::secp256k1_multiply(range_end);
            fleet[i].distance = fleet[i].is_wild ? bchaves::core::BigInt(0) : range_end;
            
            // Offset único por thread+kangaroo para evitar sobreposição
            bchaves::core::BigInt offset((uint64_t)(thread_id * 64 + i) * 1000ULL);
            start_p = bchaves::core::secp256k1_add(start_p, bchaves::core::secp256k1_multiply(offset));
            fleet[i].distance = fleet[i].distance + offset; 
            
            fleet[i].p_jac = bchaves::core::to_jacobian(start_p.x, start_p.y);
            fleet[i].p_aff = start_p;
        }

        alignas(64) bchaves::core::PointJacobian batch_j[64];
        alignas(64) bchaves::core::Secp256k1Point batch_a[64];

        while(!g_stop_requested && !found.load(std::memory_order_relaxed)) {
            // Rodada de saltos para toda a frota
            // Fase 4: Prefetch da próxima entrada da Jump Table
            for(int i=0; i<64; ++i) {
                uint32_t jump_idx = fleet[i].p_aff.x.limbs[0] % 64;
                
                // Prefetch: antecipar a próxima entrada da jump table
                if (i + 1 < 64) {
                    uint32_t next_idx = fleet[i+1].p_aff.x.limbs[0] % 64;
                    __builtin_prefetch(&g_jump_table[next_idx], 0, 3);
                }
                
                fleet[i].p_jac = bchaves::core::add_points_mixed(fleet[i].p_jac, g_jump_table[jump_idx].point);
                fleet[i].distance += g_jump_table[jump_idx].distance;
                batch_j[i] = fleet[i].p_jac;
            }

            // Normalização em massa da frota (1 mod_inv total)
            bchaves::core::batch_normalize(batch_j, batch_a, 64);
            total_hops.fetch_add(64, std::memory_order_relaxed);

            for(int i=0; i<64; ++i) {
                fleet[i].p_aff = batch_a[i];
                auto& k = fleet[i];

                // Distinguished Point: trailing 16 zero bits
                if (is_distinguished(k.p_aff.x, 16)) {
                    uint64_t h = k.p_aff.x.limbs[0];
                    int shard_idx = h % 16;

                    // Fase 3: Cuckoo pre-check (sem lock!)
                    bool maybe_exists = trap_filter->lookup(h);

                    auto& shard = shards[shard_idx];
                    std::lock_guard<std::mutex> lock(shard.mtx);

                    if (maybe_exists) {
                        // Tentar encontrar na RAM primeiro
                        KangarooTrap other;
                        bool found_in_ram = false;
                        bool found_on_disk = false;

                        auto it = shard.table.find(h);
                        if (it != shard.table.end()) {
                            other = it->second;
                            found_in_ram = true;
                        } else {
                            // Fallback: buscar no disco (a trap pode ter sido
                            // despejada em um ciclo anterior de limpeza de RAM)
                            found_on_disk = lookup_trap_on_disk(shard_idx, h, other, range_start, range_end);
                        }

                        if ((found_in_ram || found_on_disk) && other.is_wild != k.is_wild) {
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
                        // Colisão same-type ou falso positivo do Cuckoo: ignorar
                    }
                    // Inserir nova armadilha (se não existia)
                    if (!shard.table.count(h)) {
                        shard.table[h] = {k.distance, k.is_wild};
                        trap_filter->insert(h);
                        total_traps_in_ram.fetch_add(1, std::memory_order_relaxed);
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

    // ============================================================
    // Loop Principal: Monitoramento + Dump Periódico
    // ============================================================
    auto start_time = std::chrono::steady_clock::now();
    auto last_dump = start_time;
    constexpr auto DUMP_INTERVAL = std::chrono::minutes(5); // Dump a cada 5 minutos

    while(!found.load() && !g_stop_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        uint64_t current_traps = total_traps_in_ram.load();

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        double rate = static_cast<double>(total_hops.load()) / std::max<double>(1.0, (double)elapsed);
        
        std::cout << "\r[*] Total Hops: " << total_hops.load() 
                  << " | Speed: " << bchaves::system::format_rate(rate) 
                  << " | Traps: " << current_traps << " / " << max_traps << "        " << std::flush;

        // Dump periódico bufferizado (a cada 5 minutos)
        if (now - last_dump >= DUMP_INTERVAL) {
            dump_shards_to_disk(shards, range_start, range_end);
            last_dump = now;
        }

        // Dump de emergência se RAM atingir o limite
        if (current_traps >= max_traps) {
            std::cout << "\n[!] Limite de RAM atingido. Salvando armadilhas...\n";
            dump_shards_to_disk(shards, range_start, range_end);
            // Limpar a RAM para continuar (as traps ficam seguras no disco)
            for (int i = 0; i < 16; ++i) {
                std::lock_guard<std::mutex> lock(shards[i].mtx);
                shards[i].table.clear();
            }
            total_traps_in_ram.store(0);
            last_dump = now;
        }
    }

    // ============================================================
    // Shutdown: Salvar tudo antes de sair
    // ============================================================
    for(auto& t : threads) if(t.joinable()) t.join();
    std::cout << "\n";

    // Dump final de emergência (garantia de persistência)
    if (!found.load() && total_traps_in_ram.load() > 0) {
        std::cout << "[+] Salvando armadilhas antes de encerrar...\n";
        dump_shards_to_disk(shards, range_start, range_end);
    }

    if (found.load()) {
        bchaves::core::DerivedKeyInfo info;
        if (bchaves::core::derive_key_info(solution, info)) {
            bchaves::engine::report_found(info, "Kangaroo Search (range:" + options.range + ")");
        }
    }

    return 0;
}

}  // namespace bchaves::engine