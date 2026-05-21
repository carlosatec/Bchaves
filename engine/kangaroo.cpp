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
#include "core/hash_table.hpp"
#include "core/secp256k1_fleet.hpp"
#include "system/checkpoint.hpp"
#include <iostream>
#include "core/adaptive_filter.hpp"
#include <vector>
#include <immintrin.h>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include "core/flat_map.hpp"
#include <csignal>
#include <fstream>
#include <cstring>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <algorithm>

#ifdef __linux__
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <memory>

namespace bchaves::engine {


alignas(64) std::atomic<bool> g_stop_requested{false};
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

// Zero-Copy Mmap Helper
struct MappedFile {
    void* data = nullptr;
    size_t size = 0;

    MappedFile() = default;
    ~MappedFile() { unmap(); }

    bool map(const std::string& path) {
        unmap();
#ifdef __linux__
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;
        struct stat st;
        if (fstat(fd, &st) < 0 || st.st_size == 0) {
            close(fd);
            return false;
        }
        size = st.st_size;
        data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        return data != MAP_FAILED;
#else
        return false; // Fallback para outras plataformas não-linux (ex: native windows)
#endif
    }

    void unmap() {
#ifdef __linux__
        if (data && data != MAP_FAILED) munmap(data, size);
#endif
        data = nullptr;
        size = 0;
    }

    bool is_mapped() const { return data != nullptr && data != (void*)-1; }
};

struct TrapKey {
    bchaves::core::BigInt x;
    bool odd = false;

    bool operator==(const TrapKey& other) const {
        return odd == other.odd && x == other.x;
    }

    bool operator<(const TrapKey& other) const {
        if (x != other.x) return x < other.x;
        return odd < other.odd;
    }
};

struct TrapKeyHasher {
    std::size_t operator()(const TrapKey& key) const noexcept {
        std::size_t seed = key.odd ? 0x9e3779b97f4a7c15ULL : 0x85ebca6b;
        for (std::uint64_t limb : key.x.limbs) {
            seed ^= static_cast<std::size_t>(limb) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct TrapShard {
    std::mutex mtx;
    std::unique_ptr<bchaves::core::TrapTable> table;
};

static constexpr std::size_t kFleetSize = 1024;
static constexpr std::size_t kCheckpointWordsPerKangaroo = 3;

struct KangarooWorkerState {
    std::mutex mutex;
    bchaves::core::fleet::FleetState fleet;

    KangarooWorkerState() : fleet(kFleetSize) {}
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
static constexpr uint32_t TRAP_VERSION  = 3;
static constexpr size_t   TRAP_HEADER_SIZE = 4 + 4 + 32 + 32; // magic + version + range_start + range_end

// ============================================================
// Jump Table Global - Alinhada em cache line (64 bytes)
// ============================================================
alignas(64) std::array<Jump, 64> g_jump_table;

// Conversion utilities moved to core/secp256k1.hpp
using bchaves::core::bytes32_to_bigint;
using bchaves::core::to_bytes32;

static std::vector<std::array<std::uint8_t, 32>> snapshot_worker_states(
    const std::vector<std::unique_ptr<KangarooWorkerState>>& workers) {
    std::vector<std::array<std::uint8_t, 32>> snapshot;
    snapshot.reserve(workers.size() * kFleetSize * kCheckpointWordsPerKangaroo);
    for (const auto& worker : workers) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        for (std::size_t i = 0; i < kFleetSize; ++i) {
            std::array<std::uint8_t, 32> x, y, dist;
            for(int l=0; l<4; ++l) {
                for(int j=0; j<8; ++j) {
                    x[31 - (l*8 + j)] = (worker->fleet.x[l][i] >> (j*8)) & 0xFF;
                    y[31 - (l*8 + j)] = (worker->fleet.y[l][i] >> (j*8)) & 0xFF;
                    dist[31 - (l*8 + j)] = (worker->fleet.d[l][i] >> (j*8)) & 0xFF;
                }
            }
            snapshot.push_back(x);
            snapshot.push_back(y);
            snapshot.push_back(dist);
        }
    }
    return snapshot;
}

static bool restore_worker_states(const bchaves::system::CheckpointState& checkpoint,
                                  std::vector<std::unique_ptr<KangarooWorkerState>>& workers) {
    const std::size_t expected_words = workers.size() * kFleetSize * kCheckpointWordsPerKangaroo;
    if (checkpoint.worker_currents.size() != expected_words) {
        return false;
    }

    std::size_t offset = 0;
    for (auto& worker : workers) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        for (std::size_t i = 0; i < kFleetSize; ++i) {
            auto x_bytes = checkpoint.worker_currents[offset++];
            auto y_bytes = checkpoint.worker_currents[offset++];
            auto d_bytes = checkpoint.worker_currents[offset++];
            for(int l=0; l<4; ++l) {
                uint64_t xl = 0, yl = 0, dl = 0;
                for(int j=0; j<8; ++j) {
                    xl |= static_cast<uint64_t>(x_bytes[31 - (l*8+j)]) << (j*8);
                    yl |= static_cast<uint64_t>(y_bytes[31 - (l*8+j)]) << (j*8);
                    dl |= static_cast<uint64_t>(d_bytes[31 - (l*8+j)]) << (j*8);
                }
                worker->fleet.x[l][i] = xl;
                worker->fleet.y[l][i] = yl;
                worker->fleet.z[l][i] = (l == 0) ? 1 : 0; // Z = 1
                worker->fleet.d[l][i] = dl;
            }
        }
    }
    return true;
}

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

TrapKey make_trap_key(const bchaves::core::Secp256k1Point& point) {
    return {point.x, point.y.is_odd()};
}

std::uint64_t trap_filter_hash(const TrapKey& key) {
    return key.x.limbs[0];
}

bool candidate_matches_target(const bchaves::core::BigInt& candidate,
                              const bchaves::core::Secp256k1Point& target) {
    const bchaves::core::Secp256k1Point pub = bchaves::core::secp256k1_multiply(candidate);
    return !pub.infinity && pub.x == target.x && pub.y == target.y;
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

bool validate_trap_header_raw(const void* data, size_t size,
                             const bchaves::core::BigInt& range_start,
                             const bchaves::core::BigInt& range_end) {
    if (size < 4 + 4 + 32 + 32) return false;
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    
    uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
    uint32_t version = *reinterpret_cast<const uint32_t*>(ptr + 4);
    
    if (magic != TRAP_MAGIC || version != TRAP_VERSION) return false;
    
    // Verificar se o range é o mesmo
    for (int i = 0; i < 4; ++i) {
        uint64_t file_start_limb = *reinterpret_cast<const uint64_t*>(ptr + 8 + i * 8);
        uint64_t file_end_limb = *reinterpret_cast<const uint64_t*>(ptr + 40 + i * 8);
        if (file_start_limb != range_start.limbs[i]) return false;
        if (file_end_limb != range_end.limbs[i]) return false;
    }
    return true;
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
                              bchaves::core::AdaptiveCuckooFilter& filter,
                              const bchaves::core::BigInt& range_start,
                              const bchaves::core::BigInt& range_end,
                              const std::string& trap_dir) {
    uint64_t loaded = 0;
    std::error_code ec;

    if (!std::filesystem::exists(trap_dir, ec)) return 0;

    auto start_time = std::chrono::steady_clock::now();
    std::cout << "[+] Cold Boot: Carregando armadilhas do disco (Mmap Mode)...\n";

    for (int i = 0; i < 64; ++i) {
        std::string filename = trap_dir + "/shard_" + std::to_string(i) + ".bin";
        MappedFile mf;
        if (!mf.map(filename)) continue;

        if (!validate_trap_header_raw(mf.data, mf.size, range_start, range_end)) continue;

        constexpr size_t HEADER_SIZE = 4 + 4 + 32 + 32;
        constexpr size_t ENTRY_SIZE = 32 + 1 + 32 + 1;
        
        size_t num_entries = (mf.size - HEADER_SIZE) / ENTRY_SIZE;
        const uint8_t* ptr = static_cast<const uint8_t*>(mf.data) + HEADER_SIZE;

        for (size_t k = 0; k < num_entries; ++k) {
            TrapKey key;
            bchaves::core::BigInt distance;
            std::memcpy(key.x.limbs.data(), ptr, 32);
            key.odd = ptr[32] != 0;
            std::memcpy(distance.limbs.data(), ptr + 33, 32);
            bool wild = ptr[65] != 0;
            ptr += ENTRY_SIZE;

            const uint64_t hash = trap_filter_hash(key);
            int shard_idx = hash % 64;
            auto& shard = shards[shard_idx];
            std::lock_guard<std::mutex> lock(shard.mtx);
            if (!shard.table) shard.table = std::make_unique<bchaves::core::TrapTable>(1024);
            shard.table->insert(key.x, key.odd, distance, wild);
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
bool lookup_trap_on_disk(const std::string& trap_dir,
                         int shard_idx,
                         const TrapKey& target_key,
                         KangarooTrap& out_trap,
                         const bchaves::core::BigInt& range_start,
                         const bchaves::core::BigInt& range_end) {
    std::string filename = trap_dir + "/shard_" + std::to_string(shard_idx) + ".bin";
    MappedFile mf;
    if (!mf.map(filename)) return false;
    if (!validate_trap_header_raw(mf.data, mf.size, range_start, range_end)) return false;

    constexpr size_t HEADER_SIZE = 4 + 4 + 32 + 32;
    constexpr size_t ENTRY_SIZE = 32 + 1 + 32 + 1;
    
    if (mf.size < HEADER_SIZE + ENTRY_SIZE) return false;
    
    uint64_t num_entries = (mf.size - HEADER_SIZE) / ENTRY_SIZE;
    uint64_t low = 0;
    uint64_t high = num_entries - 1;
    const uint8_t* base = static_cast<const uint8_t*>(mf.data) + HEADER_SIZE;
    
    while (low <= high) {
        uint64_t mid = low + (high - low) / 2;
        const uint8_t* ptr = base + mid * ENTRY_SIZE;
        
        TrapKey key;
        std::memcpy(key.x.limbs.data(), ptr, 32);
        key.odd = ptr[32] != 0;
        
        if (key == target_key) {
            std::memcpy(out_trap.distance.limbs.data(), ptr + 33, 32);
            out_trap.is_wild = ptr[65] != 0;
            return true;
        }
        
        if (key < target_key) {
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }
    
    return false;
}

void dump_shards_to_disk(const std::string& trap_dir,
                         std::vector<TrapShard>& shards,
                         const bchaves::core::BigInt& range_start,
                         const bchaves::core::BigInt& range_end) {
    std::error_code ec;
    std::filesystem::create_directories(trap_dir, ec);

    uint64_t total_dumped = 0;

    for (int i = 0; i < 64; ++i) {
        auto& shard = shards[i];
        std::lock_guard<std::mutex> lock(shard.mtx);
        if (!shard.table || shard.table->capacity() == 0) continue;

        std::string filename = trap_dir + "/shard_" + std::to_string(i) + ".bin";

        // Merge: carregar traps existentes do disco, adicionar as da RAM, reescrever tudo.
        bchaves::core::FlatHashMap<TrapKey, KangarooTrap, TrapKeyHasher> merged;

        // 1. Carregar dados existentes do disco
        {
            std::ifstream in(filename, std::ios::binary);
            if (in.is_open() && validate_trap_header(in, range_start, range_end)) {
                while (in.good() && !in.eof()) {
                    TrapKey key;
                    bchaves::core::BigInt distance;
                    uint8_t odd = 0;
                    uint8_t wild = 0;
                    in.read(reinterpret_cast<char*>(key.x.limbs.data()), 32);
                    in.read(reinterpret_cast<char*>(&odd), 1);
                    in.read(reinterpret_cast<char*>(distance.limbs.data()), 32);
                    in.read(reinterpret_cast<char*>(&wild), 1);
                    if (!in.good()) break;
                    key.odd = odd != 0;
                    merged[key] = {distance, wild != 0};
                }
            }
        }

        // 2. Sobrescrever com dados da RAM (mais recentes)
        const auto* entries = shard.table->entries();
        for (size_t k = 0; k < shard.table->capacity(); ++k) {
            if (entries[k].flags & 1) {
                TrapKey key;
                std::memcpy(key.x.limbs.data(), entries[k].x, 32);
                key.odd = (entries[k].flags & 2) != 0;
                KangarooTrap trap;
                std::memcpy(trap.distance.limbs.data(), entries[k].dist, 32);
                trap.is_wild = (entries[k].flags & 4) != 0;
                merged[key] = trap;
            }
        }

        // 3. Ordenar chaves para permitir busca binária no disco
        std::vector<TrapKey> sorted_keys;
        sorted_keys.reserve(merged.size());
        for (auto it = merged.begin(); it != merged.end(); ++it) {
            auto [key, _] = *it;
            sorted_keys.push_back(key);
        }
        std::sort(sorted_keys.begin(), sorted_keys.end());

        // 4. Reescrever o arquivo completo com o merge ordenado
        std::ofstream out(filename, std::ios::binary | std::ios::trunc);
        if (!out) continue;

        write_trap_header(out, range_start, range_end);
        for (const auto& key : sorted_keys) {
            const auto* trap_ptr = merged.find(key);
            const auto& trap = *trap_ptr;
            out.write(reinterpret_cast<const char*>(key.x.limbs.data()), 32);
            uint8_t odd = key.odd ? 1 : 0;
            out.write(reinterpret_cast<const char*>(&odd), 1);
            out.write(reinterpret_cast<const char*>(trap.distance.limbs.data()), 32);
            uint8_t wild = trap.is_wild ? 1 : 0;
            out.write(reinterpret_cast<const char*>(&wild), 1);
        }
        total_dumped += merged.size();
    }

    std::cout << "\n[+] Dump merge: " << total_dumped
              << " armadilhas totais no disco (RAM + histórico).\n";
}



int run_kangaroo(const bchaves::system::KangarooOptions& options) {
    printf("[DEBUG] run_kangaroo entry\n");
    g_stop_requested.store(false, std::memory_order_relaxed);
    auto hardware = bchaves::system::detect_hardware();
    const std::string trap_dir = options.trap_dir.value_or(std::filesystem::path("traps")).string();
    const bool persist_traps = !options.benchmark;
    const bool load_traps = persist_traps && !options.no_load;
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
    std::string backend_error;
    if (!configure_secp256k1_backend(options.secp256k1_backend, backend_error)) {
        std::cerr << "[E] Falha ao configurar secp256k1: " << backend_error << '\n';
        return 1;
    }
    std::cout << "[+] Iniciando Kangaroo (Ultra-RAM Fleet Model)\n";
    
    bchaves::core::BigInt range_start, range_end;
    std::optional<std::uint32_t> range_bits;
    if (options.range.find("bits:") == 0) {
        uint32_t bits = std::stoul(options.range.substr(5));
        std::cout << "[*] Modo Bits Detectado: " << bits << "\n";
        range_bits = bits;
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
    uint64_t effective_ram = hw.ram_available;
    if (options.max_ram_mb > 0) {
        effective_ram = options.max_ram_mb * 1024ULL * 1024ULL;
        std::cout << "[*] Limite de RAM forçado pelo usuario: " << options.max_ram_mb << " MB\n";
    }

    uint64_t max_traps = (effective_ram * 8) / 10 / sizeof(bchaves::core::TrapEntry); // 80% da RAM real
    if (max_traps < 100000) max_traps = 100000;

    // Cuckoo Filter dimensionado para o total esperado (RAM + Disco).
    // Usamos ~10% da RAM livre para o filtro. Cada slot usa ~2 bytes (16-bit tag).
    uint64_t filter_cap = std::max((uint64_t)100000000ULL, (uint64_t)((effective_ram / 10) / 2));
    auto trap_filter = std::make_unique<bchaves::core::AdaptiveCuckooFilter>(filter_cap, hw);

    std::cout << "[+] Limite de RAM: " << (effective_ram / 1024 / 1024) << " MB\n";
    std::cout << "[+] Capacidade do Filtro: " << filter_cap / 1000000 << "M entradas\n";

    // Cálculo conservador para evitar over-allocation devido ao rounding power-of-2
    // Cada shard arredonda pra cima. Precisamos que (64 * rounded_shard_cap * entry_size) <= target_ram
    uint64_t target_ram_for_tables = (effective_ram * 7) / 10; // 70% da RAM para tabelas
    uint64_t max_entries_per_shard = target_ram_for_tables / 64 / sizeof(bchaves::core::TrapEntry);
    
    // Achar a maior potência de 2 que cabe
    uint64_t shard_cap_pow2 = 1;
    while (shard_cap_pow2 * 2 <= max_entries_per_shard) shard_cap_pow2 <<= 1;
    
    uint64_t total_max_traps = shard_cap_pow2 * 64;
    std::cout << "[+] Limite de Armadilhas em RAM: " << total_max_traps << " (64 shards de " << shard_cap_pow2 << ")\n";

    std::vector<TrapShard> shards(64);
    for(int i=0; i<64; ++i) {
        shards[i].table = std::make_unique<bchaves::core::TrapTable>(shard_cap_pow2);
    }
    std::atomic<uint64_t> total_hops{0};
    std::atomic<uint64_t> total_traps_in_ram{0};
    std::atomic<bool> found{false};
    bchaves::core::BigInt solution;
    std::mutex sol_mtx;
    std::mutex filter_mtx;



    // ============================================================
    // Fase 1: Cold Boot - Carregar armadilhas salvas anteriormente
    // ============================================================
    if (options.benchmark) {
        std::cout << "[+] --benchmark ativado: pulando carga e dump de armadilhas.\n";
    } else if (!load_traps) {
        std::cout << "[+] --no-load ativado: pulando carregamento de armadilhas do disco.\n";
    } else {
        uint64_t preloaded = load_traps_from_disk(shards, *trap_filter, range_start, range_end, trap_dir);
        total_traps_in_ram.store(preloaded);
    }

    // ============================================================
    // Worker: Fleet de 64 Kangaroos por Thread
    // ============================================================
    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    std::uint32_t num_threads = tune.threads;
    const std::filesystem::path checkpoint_path = options.checkpoint_path.value_or(
        bchaves::system::default_checkpoint_path("kangaroo", range_bits));
    std::cout << "[+] Checkpoint: " << checkpoint_path.string() << "\n";
    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    std::vector<std::unique_ptr<KangarooWorkerState>> worker_states;
    worker_states.reserve(num_threads);
    for (std::uint32_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        auto state = std::make_unique<KangarooWorkerState>();
        const uint32_t wild_count = static_cast<uint32_t>((kFleetSize * options.wild_ratio) / 100);
        for (std::size_t i = 0; i < kFleetSize; ++i) {
            const bool is_wild = static_cast<uint32_t>(i) < wild_count;
            bchaves::core::Secp256k1Point start_p = is_wild ? target_y : bchaves::core::secp256k1_multiply(range_end);
            bchaves::core::BigInt distance = is_wild ? bchaves::core::BigInt(0) : range_end;
            bchaves::core::BigInt offset(
                static_cast<std::uint64_t>(thread_id * kFleetSize + static_cast<std::uint32_t>(i)) * 1000ULL);
            start_p = bchaves::core::secp256k1_add(start_p, bchaves::core::secp256k1_multiply(offset));
            distance += offset;
            
            for(int l=0; l<4; ++l) {
                state->fleet.x[l][i] = start_p.x.limbs[l];
                state->fleet.y[l][i] = start_p.y.limbs[l];
                state->fleet.z[l][i] = (l == 0) ? 1 : 0;
                state->fleet.d[l][i] = distance.limbs[l];
            }
        }
        worker_states.push_back(std::move(state));
    }

    bchaves::system::CheckpointState checkpoint{};
    checkpoint.algorithm = "kangaroo";
    checkpoint.range_start = bchaves::core::to_bytes32(range_start);
    checkpoint.range_end = bchaves::core::to_bytes32(range_end);
    checkpoint.threads = num_threads;
    checkpoint.batch_size = static_cast<std::uint32_t>(kFleetSize);
    checkpoint.progress_secondary = options.wild_ratio;

    if (options.checkpoint_enabled && std::filesystem::exists(checkpoint_path)) {
        std::string err;
        if (bchaves::system::load_checkpoint(checkpoint_path, checkpoint, err)) {
            const bool compatible =
                checkpoint.algorithm == "kangaroo" &&
                checkpoint.threads == num_threads &&
                checkpoint.batch_size == static_cast<std::uint32_t>(kFleetSize) &&
                checkpoint.progress_secondary == options.wild_ratio &&
                checkpoint.range_start == bchaves::core::to_bytes32(range_start) &&
                checkpoint.range_end == bchaves::core::to_bytes32(range_end) &&
                restore_worker_states(checkpoint, worker_states);
            if (!compatible) {
                std::cerr << "\n[!] ERRO CRITICO DE CHECKPOINT [!]\n";
                std::cerr << "O checkpoint do kangaroo nao e compativel com os parametros atuais.\n";
                std::cerr << "Verifique range, threads, --wild/--tame ou remova o arquivo '"
                          << checkpoint_path.string() << "' para reiniciar.\n\n";
                return 1;
            }
            total_hops.store(checkpoint.progress_primary, std::memory_order_relaxed);
            std::cout << "[+] Checkpoint do kangaroo detectado. Retomando frota por thread.\n";
            std::cout << "    Hops acumulados: " << total_hops.load() << "\n";
        } else {
            std::cerr << "[!] Falha ao ler checkpoint: " << err << "\n";
        }
    }

    auto worker = [&](int thread_id) {
        bchaves::system::pin_thread_to_core(static_cast<std::uint32_t>(thread_id));
        
        KangarooWorkerState& worker_state = *worker_states[thread_id];
        bchaves::core::fleet::FleetState local_fleet(kFleetSize);
        
        // Copiar estado inicial do worker_state (SoA global) para SoA local
        {
            std::lock_guard<std::mutex> state_lock(worker_state.mutex);
            for(int l=0; l<4; ++l) {
                std::memcpy(local_fleet.x[l].data(), worker_state.fleet.x[l].data(), kFleetSize * 8);
                std::memcpy(local_fleet.y[l].data(), worker_state.fleet.y[l].data(), kFleetSize * 8);
                std::memcpy(local_fleet.z[l].data(), worker_state.fleet.z[l].data(), kFleetSize * 8);
                std::memcpy(local_fleet.d[l].data(), worker_state.fleet.d[l].data(), kFleetSize * 8);
            }
        }

        const uint32_t wild_count = static_cast<uint32_t>((kFleetSize * options.wild_ratio) / 100);
        for(size_t i=0; i<kFleetSize; ++i) local_fleet.is_wild[i] = (i < wild_count);

        const size_t lanes = bchaves::core::fleet::get_dispatcher().lanes;

        struct PendingDp {
            TrapKey key;
            bchaves::core::BigInt distance;
            bool is_wild = false;
            bool filter_maybe_present = false;
        };

        std::array<PendingDp, 64> local_dps{};
        std::size_t local_dp_count = 0;

        auto sync_local_fleet = [&]() {
            std::lock_guard<std::mutex> state_lock(worker_state.mutex);
            for(int l=0; l<4; ++l) {
                std::memcpy(worker_state.fleet.x[l].data(), local_fleet.x[l].data(), kFleetSize * 8);
                std::memcpy(worker_state.fleet.y[l].data(), local_fleet.y[l].data(), kFleetSize * 8);
                std::memcpy(worker_state.fleet.z[l].data(), local_fleet.z[l].data(), kFleetSize * 8);
                std::memcpy(worker_state.fleet.d[l].data(), local_fleet.d[l].data(), kFleetSize * 8);
            }
        };

        auto flush_local_dps = [&]() {
            if (local_dp_count == 0) return;

            {
                std::lock_guard<std::mutex> filter_lock(filter_mtx);
                for (std::size_t n = 0; n < local_dp_count; ++n) {
                    const uint64_t h = trap_filter_hash(local_dps[n].key);
                    local_dps[n].filter_maybe_present = trap_filter->lookup(h);
                    if (!local_dps[n].filter_maybe_present) {
                        trap_filter->insert(h);
                    }
                }
            }

            for (std::size_t n = 0; n < local_dp_count; ++n) {
                const uint64_t h = trap_filter_hash(local_dps[n].key);
                auto& shard = shards[h % 64];
                std::lock_guard<std::mutex> lock(shard.mtx);

                KangarooTrap other;
                if (shard.table->lookup(local_dps[n].key.x, local_dps[n].key.odd, other.distance, other.is_wild)) {
                    if (other.is_wild != local_dps[n].is_wild) {
                        bchaves::core::BigInt candidate;
                        if (local_dps[n].is_wild) {
                            candidate = range_end + other.distance - local_dps[n].distance;
                        } else {
                            candidate = range_end + local_dps[n].distance - other.distance;
                        }
                        if (candidate_matches_target(candidate, target_y)) {
                            std::lock_guard<std::mutex> slock(sol_mtx);
                            solution = candidate;
                            found = true;
                            local_dp_count = 0;
                            return;
                        }
                    }
                } else {
                    shard.table->insert(local_dps[n].key.x,
                                        local_dps[n].key.odd,
                                        local_dps[n].distance,
                                        local_dps[n].is_wild);
                    total_traps_in_ram.fetch_add(1, std::memory_order_relaxed);
                }
            }

            local_dp_count = 0;
        };

        while(!g_stop_requested && !found.load(std::memory_order_relaxed)) {
            // 1. Fase de Saltos (Vetorizada via SoA)
            for (size_t i = 0; i < kFleetSize; i += lanes) {
                // Selecionar o salto baseado no primeiro lane do sub-lote
                // (Heurística: todos os lanes do sub-lote usam o mesmo salto para permitir SIMD)
                uint32_t jump_idx = local_fleet.x[0][i] % 64;
                const auto& jump = g_jump_table[jump_idx];
                
                // 1.1 Atualizar distâncias para TODO o sub-lote
                for (size_t l = 0; l < lanes; ++l) {
                    bchaves::core::BigInt dist_i;
                    for(int limb=0; limb<4; ++limb) dist_i.limbs[limb] = local_fleet.d[limb][i+l];
                    dist_i += jump.distance;
                    for(int limb=0; limb<4; ++limb) local_fleet.d[limb][i+l] = dist_i.limbs[limb];
                }
                
                // 1.2 Kernel de Adição de Pontos (SIMD ou Escalar)
                if (lanes == 4) {
                    #if defined(__AVX2__)
                    bchaves::core::fleet::add_fleet_avx2(local_fleet, i, jump.point);
                    #else
                    // Fallback escalar se compilado sem AVX2 mas lanes detectado (não deve ocorrer)
                    for (size_t l = 0; l < 4; ++l) {
                        bchaves::core::PointJacobian p_jac;
                        for(int limb=0; limb<4; ++limb) {
                            p_jac.x.limbs[limb] = local_fleet.x[limb][i+l];
                            p_jac.y.limbs[limb] = local_fleet.y[limb][i+l];
                            p_jac.z.limbs[limb] = local_fleet.z[limb][i+l];
                        }
                        p_jac = bchaves::core::add_points_mixed(p_jac, jump.point);
                        for(int limb=0; limb<4; ++limb) {
                            local_fleet.x[limb][i+l] = p_jac.x.limbs[limb];
                            local_fleet.y[limb][i+l] = p_jac.y.limbs[limb];
                            local_fleet.z[limb][i+l] = p_jac.z.limbs[limb];
                        }
                    }
                    #endif
                } else if (lanes == 2) {
                    #if defined(__SSE4_1__)
                    bchaves::core::fleet::add_fleet_sse4(local_fleet, i, jump.point);
                    #else
                    for (size_t l = 0; l < 2; ++l) {
                        bchaves::core::PointJacobian p_jac;
                        for(int limb=0; limb<4; ++limb) {
                            p_jac.x.limbs[limb] = local_fleet.x[limb][i+l];
                            p_jac.y.limbs[limb] = local_fleet.y[limb][i+l];
                            p_jac.z.limbs[limb] = local_fleet.z[limb][i+l];
                        }
                        p_jac = bchaves::core::add_points_mixed(p_jac, jump.point);
                        for(int limb=0; limb<4; ++limb) {
                            local_fleet.x[limb][i+l] = p_jac.x.limbs[limb];
                            local_fleet.y[limb][i+l] = p_jac.y.limbs[limb];
                            local_fleet.z[limb][i+l] = p_jac.z.limbs[limb];
                        }
                    }
                    #endif
                } else {
                    // Escalar (lanes == 1)
                    bchaves::core::PointJacobian p_jac;
                    for(int limb=0; limb<4; ++limb) {
                        p_jac.x.limbs[limb] = local_fleet.x[limb][i];
                        p_jac.y.limbs[limb] = local_fleet.y[limb][i];
                        p_jac.z.limbs[limb] = local_fleet.z[limb][i];
                    }
                    p_jac = bchaves::core::add_points_mixed(p_jac, jump.point);
                    for(int limb=0; limb<4; ++limb) {
                        local_fleet.x[limb][i] = p_jac.x.limbs[limb];
                        local_fleet.y[limb][i] = p_jac.y.limbs[limb];
                        local_fleet.z[limb][i] = p_jac.z.limbs[limb];
                    }
                }
            }

            // 2. Normalização em Massa (Montgomery Batch Inversion)
            bchaves::core::fleet::batch_normalize_fleet(local_fleet);
            total_hops.fetch_add(kFleetSize, std::memory_order_relaxed);

            // 3. Verificação de Distinguished Points e Armadilhas (SIMD Masking)
            //    Macro para o processamento individual de um DP encontrado na posição idx
            #define PROCESS_DISTINGUISHED_POINT(idx) do { \
                PendingDp& dp = local_dps[local_dp_count++]; \
                for(int l=0; l<4; ++l) { \
                    dp.key.x.limbs[l] = local_fleet.x[l][idx]; \
                    dp.distance.limbs[l] = local_fleet.d[l][idx]; \
                } \
                dp.key.odd = (local_fleet.y[0][idx] & 1); \
                dp.is_wild = local_fleet.is_wild[idx]; \
                dp.filter_maybe_present = false; \
                if (local_dp_count == local_dps.size()) flush_local_dps(); \
            } while(0)

#if defined(__AVX512F__)
            {
                const __m512i mask16 = _mm512_set1_epi64(0xFFFFULL);
                const __m512i zero = _mm512_setzero_si512();
                for (size_t i = 0; i < kFleetSize; i += 8) {
                    __m512i x0 = _mm512_loadu_si512(&local_fleet.x[0][i]);
                    __m512i masked = _mm512_and_si512(x0, mask16);
                    __mmask8 hits = _mm512_cmpeq_epi64_mask(masked, zero);
                    if (hits == 0) continue; // Fast path: nenhum DP neste bloco de 8
                    // Slow path: extrair posições dos bits ativos
                    while (hits) {
                        int bit = __builtin_ctz(hits);
                        size_t idx = i + bit;
                        PROCESS_DISTINGUISHED_POINT(idx);
                        hits &= hits - 1;
                    }
                }
            }
#elif defined(__AVX2__)
            {
                const __m256i mask16 = _mm256_set1_epi64x(0xFFFFULL);
                const __m256i zero = _mm256_setzero_si256();
                for (size_t i = 0; i < kFleetSize; i += 4) {
                    __m256i x0 = _mm256_loadu_si256((__m256i*)&local_fleet.x[0][i]);
                    __m256i masked = _mm256_and_si256(x0, mask16);
                    __m256i cmp = _mm256_cmpeq_epi64(masked, zero);
                    int movmask = _mm256_movemask_epi8(cmp);
                    if (movmask == 0) continue; // Fast path: nenhum DP neste bloco de 4
                    // Slow path: extrair posições dos bits ativos
                    for (int lane = 0; lane < 4; ++lane) {
                        if (movmask & (0xFF << (lane * 8))) {
                            size_t idx = i + lane;
                            PROCESS_DISTINGUISHED_POINT(idx);
                        }
                    }
                }
            }
#else
            // Fallback escalar
            for (size_t i = 0; i < kFleetSize; ++i) {
                if ((local_fleet.x[0][i] & 0xFFFFULL) == 0) {
                    PROCESS_DISTINGUISHED_POINT(i);
                }
            }
#endif
            #undef PROCESS_DISTINGUISHED_POINT
            flush_local_dps();
            
            // Sincronizar periodicamente com o estado global para checkpoints
            if (total_hops.load() % (kFleetSize * 100) == 0) {
                sync_local_fleet();
            }
        }

        flush_local_dps();
        sync_local_fleet();
    };

    std::vector<std::thread> threads;
    for(std::uint32_t i=0; i<num_threads; ++i) threads.emplace_back(worker, i);

    // ============================================================
    // Loop Principal: Monitoramento + Dump Periódico
    // ============================================================
    auto start_time = std::chrono::steady_clock::now();
    auto last_dump = start_time;
    auto last_checkpoint = start_time;
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

        if (persist_traps && now - last_dump >= DUMP_INTERVAL) {
            dump_shards_to_disk(trap_dir, shards, range_start, range_end);
            last_dump = now;
        }

        if (options.checkpoint_enabled &&
            !options.benchmark &&
            now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            if (persist_traps && total_traps_in_ram.load(std::memory_order_relaxed) > 0) {
                dump_shards_to_disk(trap_dir, shards, range_start, range_end);
                last_dump = now;
            }
            bchaves::system::CheckpointState ckp{};
            ckp.algorithm = "kangaroo";
            ckp.range_start = bchaves::core::to_bytes32(range_start);
            ckp.range_end = bchaves::core::to_bytes32(range_end);
            ckp.threads = num_threads;
            ckp.batch_size = static_cast<std::uint32_t>(kFleetSize);
            ckp.progress_primary = total_hops.load(std::memory_order_relaxed);
            ckp.progress_secondary = options.wild_ratio;
            ckp.worker_currents = snapshot_worker_states(worker_states);
            ckp.timestamp = static_cast<std::uint64_t>(std::time(nullptr));
            std::string err;
            if (!bchaves::system::save_checkpoint(checkpoint_path, ckp, err)) {
                std::cerr << "\n[!] Falha ao salvar checkpoint do kangaroo: " << err << "\n";
            }
            last_checkpoint = now;
        }

        // Dump de emergência se RAM atingir o limite
        if (current_traps >= max_traps) {
            if (persist_traps) {
                std::cout << "\n[!] Limite de RAM atingido. Salvando armadilhas...\n";
                dump_shards_to_disk(trap_dir, shards, range_start, range_end);
            } else {
                std::cout << "\n[!] Limite de RAM atingido em benchmark. Limpando armadilhas em memoria...\n";
            }
            for (int i = 0; i < 64; ++i) {
                std::lock_guard<std::mutex> lock(shards[i].mtx);
                if (shards[i].table) shards[i].table->clear();
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
    if (persist_traps && !found.load() && total_traps_in_ram.load() > 0) {
        std::cout << "[+] Salvando armadilhas antes de encerrar...\n";
        dump_shards_to_disk(trap_dir, shards, range_start, range_end);
    }

    if (g_stop_requested && !found.load() && options.checkpoint_enabled && !options.benchmark) {
        bchaves::system::CheckpointState ckp{};
        ckp.algorithm = "kangaroo";
        ckp.range_start = bchaves::core::to_bytes32(range_start);
        ckp.range_end = bchaves::core::to_bytes32(range_end);
        ckp.threads = num_threads;
        ckp.batch_size = static_cast<std::uint32_t>(kFleetSize);
        ckp.progress_primary = total_hops.load(std::memory_order_relaxed);
        ckp.progress_secondary = options.wild_ratio;
        ckp.worker_currents = snapshot_worker_states(worker_states);
        ckp.timestamp = static_cast<std::uint64_t>(std::time(nullptr));
        std::string err;
        if (bchaves::system::save_checkpoint(checkpoint_path, ckp, err)) {
            std::cout << "[+] Checkpoint do kangaroo salvo com sucesso.\n";
        } else {
            std::cerr << "[!] Falha ao salvar checkpoint final do kangaroo: " << err << "\n";
        }
    }

    if (found.load()) {
        bchaves::core::DerivedKeyInfo info;
        if (bchaves::core::derive_key_info(solution, info)) {
            bchaves::engine::report_found(
                info,
                "Kangaroo Search (range:" + options.range + ")",
                !options.benchmark);
        }
    }

    return 0;
}

}  // namespace bchaves::engine
