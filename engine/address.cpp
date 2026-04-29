/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: O núcleo do motor de busca de endereços (Linear + Hybrid Chunk).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "engine/app.hpp"

#include "core/address.hpp"
#include "core/secp256k1.hpp"
#include "system/checkpoint.hpp"
#include "system/format.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <iomanip> // Adicionado para setprecision

namespace bchaves::engine {


volatile std::sig_atomic_t g_interrupt_requested = 0;

void handle_signal(int) {
    g_interrupt_requested = 1;
}

// ===== HYBRID: divisão BigInt exata =====
static bool bigint_div_u64_checked(const bchaves::core::BigInt& num, uint64_t denom, uint64_t& out) {
    if (denom == 0) return false;
    __uint128_t rem = 0;
    uint64_t result[4] = {};
    for (int i = 3; i >= 0; --i) {
        const __uint128_t cur = (rem << 64) | num.limbs[i];
        result[i] = static_cast<uint64_t>(cur / denom);
        rem = cur % denom;
    }
    if (result[1] != 0 || result[2] != 0 || result[3] != 0) return false;
    out = result[0];
    return true;
}

// ===== HYBRID: LCG bijetor =====
static uint64_t gcd64(uint64_t a, uint64_t b) {
    while (b != 0) { const uint64_t t = b; b = a % b; a = t; }
    return a;
}

static uint64_t find_coprime_step(uint64_t n) {
    uint64_t step = 0x9e3779b97f4a7c15ULL;
    while (gcd64(step, n) != 1) ++step;
    return step;
}

// ===== HYBRID: estado global atômico =====
static std::atomic<uint64_t> g_chunk_counter{0};
static uint64_t g_hybrid_chunk_size   = 0;
static uint64_t g_hybrid_total_chunks = 0;
static uint64_t g_chunk_step          = 0;

struct SequentialWorkerState {
    std::mutex mutex;
    bchaves::core::BigInt next_key{};
};

static bchaves::core::BigInt bytes32_to_bigint(const std::array<std::uint8_t, 32>& bytes) {
    bchaves::core::BigInt out;
    for (int i = 0; i < 4; ++i) {
        std::uint64_t limb = 0;
        for (int j = 0; j < 8; ++j) {
            limb |= static_cast<std::uint64_t>(bytes[31 - (i * 8 + j)]) << (j * 8);
        }
        out.limbs[i] = limb;
    }
    return out;
}

static std::vector<std::array<std::uint8_t, 32>> snapshot_worker_currents(
    const std::vector<std::unique_ptr<SequentialWorkerState>>& workers) {
    std::vector<std::array<std::uint8_t, 32>> snapshot;
    snapshot.reserve(workers.size());
    for (const auto& worker : workers) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        snapshot.push_back(bchaves::core::to_bytes32(worker->next_key));
    }
    return snapshot;
}

static bool matcher_contains_hash(const AddressMatcher& matcher, const std::uint8_t* hash160) {
    std::array<std::uint8_t, 20> candidate{};
    std::memcpy(candidate.data(), hash160, candidate.size());
    return std::binary_search(matcher.hashes.begin(), matcher.hashes.end(), candidate);
}

bool load_targets(const std::filesystem::path& path, AddressMatcher& matcher) {
    std::cout << "[+] Carregando: " << path.string() << '\n';
    auto result = bchaves::system::load_targets(path, false);
    if (result.entries.empty()) return false;
    
    matcher.filter = std::make_unique<bchaves::core::CuckooFilter>(result.entries.size());
    for (const auto& entry : result.entries) {
        if (entry.payload.size() == 20) {
             std::array<std::uint8_t, 20> h;
             std::copy(entry.payload.begin(), entry.payload.end(), h.begin());
             matcher.hashes.push_back(h);
             uint64_t filter_hash;
             std::memcpy(&filter_hash, h.data(), sizeof(uint64_t));
             matcher.filter->insert(filter_hash);
        }
    }
    std::sort(matcher.hashes.begin(), matcher.hashes.end());
    std::cout << "[+] Alvos (Hash160): " << matcher.hashes.size() << " [Lookup O(log N) ativado]\n";
    return !matcher.hashes.empty();
}



bool resolve_range(const bchaves::system::AddressOptions& options,
                bchaves::core::BigInt& start,
                bchaves::core::BigInt& end) {
    if (options.bits == 0 || options.bits > 256) {
        std::cerr << "[E] Use -b <bits> (1-256)\n";
        return false;
    }
    start = bchaves::core::BigInt(0);
    end = bchaves::core::BigInt(0);
    
    // start = 2^(bits-1)
    if (options.bits == 1) {
        start.limbs[0] = 1;
    } else {
        std::size_t bit_idx = options.bits - 1;
        start.limbs[bit_idx / 64] = (1ULL << (bit_idx % 64));
    }

    // end = 2^(bits) - 1
    for (std::uint32_t i = 0; i < options.bits; ++i) {
        end.limbs[i / 64] |= (1ULL << (i % 64));
    }
    
    std::cout << "[+] Bit range: " << options.bits << " bits\n";
    return true;
}

void run_hybrid_worker(
    std::uint32_t              /*tid*/,
    const bchaves::core::BigInt& range_start,
    const bchaves::core::BigInt& range_end,
    const AddressMatcher&        matcher,
    const bchaves::system::AddressOptions& options,
    std::atomic<uint64_t>&       total_keys_processed,
    std::atomic<uint64_t>&       total_checks_processed,
    bchaves::core::DerivedKeyInfo& found_key,
    std::atomic<bool>&           found,
    std::mutex&                  found_mutex,
    volatile std::sig_atomic_t&  interrupt)
{
    static constexpr size_t kBatch = 1024;

    const auto G  = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(1));
    const auto bG = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(kBatch));
    const bchaves::core::BigInt big_chunk(g_hybrid_chunk_size);
    const bchaves::core::BigInt batch_step(kBatch);

    static bchaves::core::Secp256k1Point Gn[512];
    static bchaves::core::Secp256k1Point _2Gn;
    static std::once_flag init_gn;
    std::call_once(init_gn, [&](){
        bchaves::core::PointJacobian j = bchaves::core::to_jacobian(G.x, G.y);
        bchaves::core::PointJacobian g_batch[512];
        for(int i=0; i<512; ++i) {
            g_batch[i] = j;
            j = bchaves::core::add_points_mixed(j, G);
        }
        bchaves::core::batch_normalize(g_batch, Gn, 512);
        
        bchaves::core::PointJacobian j1024 = bchaves::core::to_jacobian(bG.x, bG.y);
        bchaves::core::batch_normalize(&j1024, &_2Gn, 1);
    });

        bool do_compress = options.type == bchaves::system::SearchType::compress || options.type == bchaves::system::SearchType::both;
        bool do_uncompress = options.type == bchaves::system::SearchType::uncompress || options.type == bchaves::system::SearchType::both;
        const bool calculate_y = true; // MUST always be true to correctly evaluate parity in endomorphism reconstuction

    bchaves::core::Secp256k1Point batch_affine[8];
    uint32_t batch_offsets[8];
    int batch_count = 0;

    std::uint8_t batch_sha_out[8][32];
    std::uint8_t batch_ripemd_out[8][20];
    std::uint8_t* sha_ptr[8];
    const std::uint8_t* sha_in_ptr[8];
    std::uint8_t* ripemd_ptr[8];
    const std::uint8_t* data_ptr[8];
    alignas(32) std::uint8_t pub_bufs[8][65];

    while (!interrupt && !found.load(std::memory_order_relaxed)) {
        const uint64_t my_idx = g_chunk_counter.fetch_add(1, std::memory_order_relaxed);
        if (my_idx >= g_hybrid_total_chunks) break;
        
        // LCG puro: gcd(g_chunk_step, g_hybrid_total_chunks) == 1
        // garante bijeção (cobertura total sem repetições).
        const uint64_t chunk_id = (my_idx * g_chunk_step) % g_hybrid_total_chunks;

        bchaves::core::BigInt cur_key = range_start;
        cur_key += bchaves::core::BigInt(chunk_id) * big_chunk;
        uint64_t chunk_key_count = g_hybrid_chunk_size;
        if (chunk_id + 1 == g_hybrid_total_chunks) {
            bchaves::core::BigInt remaining = range_end - cur_key;
            ++remaining;
            std::uint64_t remaining_u64 = 0;
            if (!bchaves::core::bigint_to_u64(remaining, remaining_u64)) {
                remaining_u64 = g_hybrid_chunk_size;
            }
            chunk_key_count = std::min(chunk_key_count, remaining_u64);
        }
        if (chunk_key_count == 0) continue;

        bchaves::core::BigInt center_key = cur_key + bchaves::core::BigInt(512);
        bchaves::core::Secp256k1Point startP = bchaves::core::secp256k1_multiply(center_key);

        bchaves::core::BigInt dx[513];
        bchaves::core::BigInt dx_inv[513];

        for (uint64_t done = 0;
             done < chunk_key_count && !interrupt && !found.load(std::memory_order_relaxed);
             done += kBatch)
        {
            const uint32_t valid_offsets_limit =
                static_cast<uint32_t>(std::min<uint64_t>(kBatch, chunk_key_count - done));

            for(int i=0; i<512; ++i) {
                dx[i] = bchaves::core::mod_sub(Gn[i].x, startP.x, bchaves::core::kFieldPrime);
            }
            dx[512] = bchaves::core::mod_sub(_2Gn.x, startP.x, bchaves::core::kFieldPrime);
            
            bchaves::core::batch_mod_inv_k1(dx, 513, dx_inv);

            auto check_batch_fn = [&](const bchaves::core::Secp256k1Point* pts, int lane_count, int endo_variant) {
                // Lambda factors for reconstruction
                bchaves::core::BigInt lambda_factor;
                if (endo_variant == 0) lambda_factor = 1;
                else if (endo_variant == 1) lambda_factor = bchaves::core::kGLV_Lambda;
                else if (endo_variant == 2) lambda_factor = bchaves::core::kGLV_Lambda2;

                auto hash_and_check = [&](bool is_compress, int parity_override) {
                    size_t p_len = is_compress ? 33 : 65;
                    for(int u=0; u<lane_count; ++u) {
                        data_ptr[u] = pub_bufs[u];
                        sha_ptr[u] = batch_sha_out[u];
                        sha_in_ptr[u] = batch_sha_out[u];
                        ripemd_ptr[u] = batch_ripemd_out[u];

                        if (is_compress) {
                            pub_bufs[u][0] = parity_override;
                            uint64_t swapped[4] = {
                                __builtin_bswap64(pts[u].x.limbs[3]),
                                __builtin_bswap64(pts[u].x.limbs[2]),
                                __builtin_bswap64(pts[u].x.limbs[1]),
                                __builtin_bswap64(pts[u].x.limbs[0])
                            };
                            std::memcpy(pub_bufs[u] + 1, swapped, 32);
                        } else {
                            pub_bufs[u][0] = 0x04;
                            uint64_t swapped_x[4] = {
                                __builtin_bswap64(pts[u].x.limbs[3]),
                                __builtin_bswap64(pts[u].x.limbs[2]),
                                __builtin_bswap64(pts[u].x.limbs[1]),
                                __builtin_bswap64(pts[u].x.limbs[0])
                            };
                            std::memcpy(pub_bufs[u] + 1, swapped_x, 32);
                            
                            bchaves::core::BigInt y_val = pts[u].y;
                            if (parity_override == 3) {
                                y_val = bchaves::core::mod_sub(bchaves::core::kFieldPrime, y_val, bchaves::core::kFieldPrime);
                            }
                            uint64_t swapped_y[4] = {
                                __builtin_bswap64(y_val.limbs[3]),
                                __builtin_bswap64(y_val.limbs[2]),
                                __builtin_bswap64(y_val.limbs[1]),
                                __builtin_bswap64(y_val.limbs[0])
                            };
                            std::memcpy(pub_bufs[u] + 33, swapped_y, 32);
                        }
                    }
                    
                    if (lane_count == 8) {
                        bchaves::core::Sha256::hash8(data_ptr, p_len, sha_ptr);
                        bchaves::core::ripemd160_batch8(sha_in_ptr, 32, ripemd_ptr);
                    } else {
                        for (int u = 0; u < lane_count; ++u) {
                            const auto sha = bchaves::core::sha256(pub_bufs[u], p_len);
                            std::memcpy(batch_sha_out[u], sha.data(), sha.size());
                            const auto ripemd = bchaves::core::ripemd160(batch_sha_out[u], 32);
                            std::memcpy(batch_ripemd_out[u], ripemd.data(), ripemd.size());
                        }
                    }

                    for(int u=0; u<lane_count; ++u) {
                        uint64_t filter_hash;
                        std::memcpy(&filter_hash, batch_ripemd_out[u], sizeof(uint64_t));
                        if (matcher.filter && !matcher.filter->lookup(filter_hash)) continue;

                        std::array<uint8_t, 20> current_hash;
                        std::memcpy(current_hash.data(), batch_ripemd_out[u], 20);

                        if (std::binary_search(matcher.hashes.begin(), matcher.hashes.end(), current_hash)) {
                            // Encontrou algum alvo.
                            std::lock_guard<std::mutex> lock(found_mutex);
                                if (!found.load()) {
                                    bchaves::core::BigInt match_key = cur_key + bchaves::core::BigInt(batch_offsets[u]);
                                    
                                    if (endo_variant != 0) {
                                        match_key = bchaves::core::mod_mul(match_key, lambda_factor, bchaves::core::kCurveOrder);
                                    }
                                    
                                    if (endo_variant == 0) {
                                        // Para o base, o pts[u].y gerado inicialmente dita qual a paridade verdadeira para `match_key`
                                        // Se estamos checando uma paridade *diferente* do pts[u].y, então é -k
                                        bool real_is_odd = pts[u].y.is_odd();
                                        bool checked_is_odd = (parity_override == 0x03 || parity_override == 3);
                                        if (real_is_odd != checked_is_odd) {
                                            match_key = bchaves::core::kCurveOrder - match_key;
                                        }
                                    } else {
                                        // Para endo, a gente tem um Y que também pode ser diferente.
                                        // Em teoria, temos que gerar o Y verdadeiro do endo para saber se é k ou -k.
                                        // A forma mais segura é gerar o ponto público de match_key e comparar!
                                        bchaves::core::Secp256k1Point pub = bchaves::core::secp256k1_multiply(match_key);
                                        bool match_is_odd = pub.y.is_odd();
                                        bool target_is_odd = (parity_override == 0x03 || parity_override == 3);
                                        if (match_is_odd != target_is_odd) {
                                            match_key = bchaves::core::kCurveOrder - match_key;
                                        }
                                    }

                                    bchaves::core::derive_key_info(match_key, found_key);
                                    found = true;
                                }
                        }
                    }
                };

                if (do_compress) {
                    hash_and_check(true, 0x02);
                    hash_and_check(true, 0x03);
                }
                if (do_uncompress) {
                    hash_and_check(false, 2); // 2 means original Y
                    hash_and_check(false, 3); // 3 means -Y
                }
            };

            auto process_batch = [&](int lane_count) {
                if (lane_count <= 0) return;

                check_batch_fn(batch_affine, lane_count, 0);

                if (options.endomorphism) {
                    bchaves::core::Secp256k1Point endo1[8];
                    bchaves::core::Secp256k1Point endo2[8];
                    for(int u=0; u<lane_count; ++u) {
                        endo1[u].x = bchaves::core::mod_mul_k1(batch_affine[u].x, bchaves::core::kGLV_Beta);
                        endo1[u].y = batch_affine[u].y; // y is not actually used correctly for endo, but parity check fixes it
                        
                        endo2[u].x = bchaves::core::mod_mul_k1(batch_affine[u].x, bchaves::core::kGLV_Beta2);
                        endo2[u].y = batch_affine[u].y;
                    }
                    check_batch_fn(endo1, lane_count, 1);
                    check_batch_fn(endo2, lane_count, 2);
                }
            };

            auto push_point = [&](const bchaves::core::BigInt& px, const bchaves::core::BigInt& py, uint32_t offset) {
                if (offset >= valid_offsets_limit) return;
                batch_affine[batch_count].x = px;
                batch_affine[batch_count].y = py;
                batch_affine[batch_count].infinity = false;
                batch_offsets[batch_count] = offset;
                batch_count++;
                if (batch_count == 8) {
                    process_batch(batch_count);
                    batch_count = 0;
                }
            };
            
            push_point(startP.x, startP.y, 512);

            for(int i=0; i<511; ++i) {
                bchaves::core::BigInt dy, _s, _p, dyn;
                
                // pp = startP + Gn[i]
                dy = bchaves::core::mod_sub(Gn[i].y, startP.y, bchaves::core::kFieldPrime);
                _s = bchaves::core::mod_mul_k1(dy, dx[i]);
                _p = bchaves::core::mod_square_k1(_s);
                bchaves::core::BigInt pp_x = bchaves::core::mod_sub(bchaves::core::mod_sub(_p, startP.x, bchaves::core::kFieldPrime), Gn[i].x, bchaves::core::kFieldPrime);
                bchaves::core::BigInt pp_y;
                if (calculate_y) {
                    pp_y = bchaves::core::mod_sub(bchaves::core::mod_mul_k1(bchaves::core::mod_sub(Gn[i].x, pp_x, bchaves::core::kFieldPrime), _s), Gn[i].y, bchaves::core::kFieldPrime);
                }
                push_point(pp_x, pp_y, 512 + i + 1);

                // pn = startP - Gn[i]
                dyn = bchaves::core::mod_sub(bchaves::core::kFieldPrime, Gn[i].y, bchaves::core::kFieldPrime);
                dyn = bchaves::core::mod_sub(dyn, startP.y, bchaves::core::kFieldPrime);
                _s = bchaves::core::mod_mul_k1(dyn, dx[i]);
                _p = bchaves::core::mod_square_k1(_s);
                bchaves::core::BigInt pn_x = bchaves::core::mod_sub(bchaves::core::mod_sub(_p, startP.x, bchaves::core::kFieldPrime), Gn[i].x, bchaves::core::kFieldPrime);
                bchaves::core::BigInt pn_y;
                if (calculate_y) {
                    pn_y = bchaves::core::mod_add(bchaves::core::mod_mul_k1(bchaves::core::mod_sub(Gn[i].x, pn_x, bchaves::core::kFieldPrime), _s), Gn[i].y, bchaves::core::kFieldPrime);
                }
                push_point(pn_x, pn_y, 512 - i - 1);
            }

            {
                int i = 511;
                bchaves::core::BigInt dyn = bchaves::core::mod_sub(bchaves::core::kFieldPrime, Gn[i].y, bchaves::core::kFieldPrime);
                dyn = bchaves::core::mod_sub(dyn, startP.y, bchaves::core::kFieldPrime);
                bchaves::core::BigInt _s = bchaves::core::mod_mul_k1(dyn, dx[i]);
                bchaves::core::BigInt _p = bchaves::core::mod_square_k1(_s);
                bchaves::core::BigInt pn_x = bchaves::core::mod_sub(bchaves::core::mod_sub(_p, startP.x, bchaves::core::kFieldPrime), Gn[i].x, bchaves::core::kFieldPrime);
                bchaves::core::BigInt pn_y;
                if (calculate_y) {
                    pn_y = bchaves::core::mod_add(bchaves::core::mod_mul_k1(bchaves::core::mod_sub(Gn[i].x, pn_x, bchaves::core::kFieldPrime), _s), Gn[i].y, bchaves::core::kFieldPrime);
                }
                push_point(pn_x, pn_y, 0);
            }

            if (batch_count > 0) {
                process_batch(batch_count);
                batch_count = 0;
            }

            if (found.load(std::memory_order_relaxed) || interrupt) break;

            {
                bchaves::core::BigInt dy = bchaves::core::mod_sub(_2Gn.y, startP.y, bchaves::core::kFieldPrime);
                bchaves::core::BigInt _s = bchaves::core::mod_mul_k1(dy, dx[512]);
                bchaves::core::BigInt _p = bchaves::core::mod_square_k1(_s);
                bchaves::core::BigInt next_x = bchaves::core::mod_sub(bchaves::core::mod_sub(_p, startP.x, bchaves::core::kFieldPrime), _2Gn.x, bchaves::core::kFieldPrime);
                bchaves::core::BigInt next_y = bchaves::core::mod_sub(bchaves::core::mod_mul_k1(bchaves::core::mod_sub(_2Gn.x, next_x, bchaves::core::kFieldPrime), _s), _2Gn.y, bchaves::core::kFieldPrime);
                startP.x = next_x;
                startP.y = next_y;
            }

            cur_key += batch_step;
            
            uint64_t mult = 2;
            if (options.type == bchaves::system::SearchType::both) mult = 4;
            if (options.endomorphism) mult *= 3;
            total_keys_processed.fetch_add(valid_offsets_limit, std::memory_order_relaxed);
            total_checks_processed.fetch_add(static_cast<uint64_t>(valid_offsets_limit) * mult, std::memory_order_relaxed);
        }
    }
}


int run_address(const bchaves::system::AddressOptions& options) {
    auto hardware = bchaves::system::detect_hardware();
    const bool checkpoint_enabled = options.checkpoint_enabled && !options.benchmark;
    const bool persist_results = !options.benchmark;
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

    std::signal(SIGINT, handle_signal);

    std::string backend_error;
    if (!configure_secp256k1_backend(options.secp256k1_backend, backend_error)) {
        std::cerr << "[E] Falha ao configurar secp256k1: " << backend_error << '\n';
        return 1;
    }

    AddressMatcher matcher;
    if (!load_targets(options.target_path, matcher)) return 1;

    bchaves::core::BigInt start, end;
    if (!resolve_range(options, start, end)) return 1;

    auto tune = bchaves::system::tune_for(hardware, options.auto_tune, options.threads);
    std::uint32_t num_threads = tune.threads;
    std::cout << "[+] Perfil: " << bchaves::system::to_string(options.auto_tune) << " | Threads: " << num_threads << '\n';

    auto started = std::chrono::steady_clock::now();
    std::atomic<uint64_t> total_processed{0};
    std::atomic<uint64_t> total_checks_processed{0};
    std::atomic<bool> found{false};
    bchaves::core::DerivedKeyInfo found_key;
    std::mutex found_mutex;

    // TODO: Dividir range entre threads e implementar worker loop P = P + G
    // Por enquanto, implementamos um despachante simples para validar o -t
    
    const std::string checkpoint_name = "address_" + std::to_string(options.bits) + "bit.ckp";
    const std::filesystem::path sequential_checkpoint_path =
        options.checkpoint_path.value_or(std::filesystem::path(checkpoint_name));
    const auto hybrid_checkpoint_path = [&]() {
        return options.checkpoint_path.value_or(
            bchaves::system::default_checkpoint_path("address-hybrid", options.bits));
    };
    bchaves::system::CheckpointState checkpoint{};
    checkpoint.algorithm = "address";
    checkpoint.range_start = bchaves::core::to_bytes32(start);
    checkpoint.range_end = bchaves::core::to_bytes32(end);
    checkpoint.mode = options.mode;
    checkpoint.type = options.type;
    checkpoint.threads = num_threads;
    checkpoint.batch_size = tune.batch_size;

    if (options.mode == bchaves::system::SearchMode::hybrid) {
        static constexpr uint64_t kBatch = 1024;
        g_hybrid_chunk_size = static_cast<uint64_t>(kBatch) * options.chunk_k;
        if (g_hybrid_chunk_size < 1048576ULL) g_hybrid_chunk_size = 1048576ULL;

        bchaves::core::BigInt diff = end - start;
        ++diff;
        bchaves::core::BigInt remainder_adj(g_hybrid_chunk_size - 1);
        diff = diff + remainder_adj;
        if (!bigint_div_u64_checked(diff, g_hybrid_chunk_size, g_hybrid_total_chunks)) {
            std::cerr << "\n[!] ERRO: o particionamento hybrid excede o limite atual de 64 bits para indice de chunks.\n";
            std::cerr << "    Ajuste '-k' para reduzir o numero total de chunks ou use um range menor.\n\n";
            return 1;
        }
        if (g_hybrid_total_chunks == 0) g_hybrid_total_chunks = 1;

        bool resuming = false;
        if (checkpoint_enabled) {
            const auto ckp_path = hybrid_checkpoint_path();
            if (std::filesystem::exists(ckp_path)) {
                std::string err;
                if (bchaves::system::load_checkpoint(ckp_path, checkpoint, err)) {
                    if (checkpoint.algorithm == "address-hybrid"
                        && checkpoint.hybrid_chunk_size   == g_hybrid_chunk_size
                        && checkpoint.hybrid_total_chunks == g_hybrid_total_chunks) {
                        
                        g_chunk_step = checkpoint.hybrid_chunk_step;
                        if (g_chunk_step == 0 || gcd64(g_chunk_step, g_hybrid_total_chunks) != 1)
                            g_chunk_step = find_coprime_step(g_hybrid_total_chunks);
                        g_chunk_counter.store(checkpoint.hybrid_chunk_counter);
                        resuming = true;
                        std::cout << "[+] Checkpoint detectado. Retomando progresso...\n";
                        std::cout << "    Progresso: " << checkpoint.hybrid_chunk_counter
                                  << " / " << g_hybrid_total_chunks << " chunks\n";
                    } else {
                        std::cerr << "\n[!] ERRO CRÍTICO DE CHECKPOINT [!]\n";
                        std::cerr << "O arquivo de checkpoint existente foi criado com parâmetros incompatíveis.\n";
                        std::cerr << "Você provavelmente alterou o valor de '-k' ou o Modo de Busca (-R).\n";
                        std::cerr << "No modo Híbrido, o mapa de progresso não pode ser traduzido.\n";
                        std::cerr << "Para proteger seu progresso anterior, a execução foi ABORTADA.\n";
                        std::cerr << "-> Use os mesmos parâmetros originais para continuar,\n";
                        std::cerr << "-> OU apague o arquivo '" << ckp_path << "' para recomeçar do zero.\n\n";
                        exit(1);
                    }
                } else {
                    std::cerr << "[!] Falha ao ler checkpoint: " << err << "\n";
                }
            }
        }
        if (!resuming) {
            g_chunk_step = find_coprime_step(g_hybrid_total_chunks);
            g_chunk_counter.store(0);
        }
        std::cout << "[+] Hybrid: chunk=" << g_hybrid_chunk_size
                  << " total=" << g_hybrid_total_chunks
                  << " step=" << g_chunk_step << "\n";
    } else {
        checkpoint.algorithm = "address-sequential";
        if (checkpoint_enabled && std::filesystem::exists(sequential_checkpoint_path)) {
            std::string err;
            if (bchaves::system::load_checkpoint(sequential_checkpoint_path, checkpoint, err)) {
                const bool compatible =
                    checkpoint.algorithm == "address-sequential" &&
                    checkpoint.mode == bchaves::system::SearchMode::sequential &&
                    checkpoint.type == options.type &&
                    checkpoint.threads == num_threads &&
                    checkpoint.batch_size == tune.batch_size &&
                    checkpoint.worker_currents.size() == num_threads;
                if (!compatible) {
                    std::cerr << "\n[!] ERRO CRITICO DE CHECKPOINT [!]\n";
                    std::cerr << "O checkpoint sequencial existente nao e compativel com a retomada exata.\n";
                    std::cerr << "Verifique modo, tipo, threads, batch ou remova o arquivo '"
                              << sequential_checkpoint_path.string() << "' para reiniciar.\n\n";
                    return 1;
                }
                total_processed = checkpoint.progress_primary;
                std::cout << "[+] Checkpoint sequencial detectado. Retomando estados exatos por thread.\n";
                std::cout << "    Progresso: " << total_processed.load() << " chaves processadas\n";
            } else {
                std::cerr << "[!] Falha ao ler checkpoint: " << err << "\n";
            }
        }
    }

    std::vector<std::thread> workers;
    std::atomic<std::uint32_t> active_workers{0};
    const size_t kBatchSize = tune.batch_size;
    std::vector<std::unique_ptr<SequentialWorkerState>> sequential_worker_states;
    if (options.mode != bchaves::system::SearchMode::hybrid) {
        sequential_worker_states.reserve(num_threads);
        for (std::uint32_t i = 0; i < num_threads; ++i) {
            auto state = std::make_unique<SequentialWorkerState>();
            if (checkpoint.worker_currents.size() == num_threads) {
                state->next_key = bytes32_to_bigint(checkpoint.worker_currents[i]);
            } else {
                state->next_key = start + (bchaves::core::BigInt(i) * bchaves::core::BigInt(num_threads));
            }
            sequential_worker_states.push_back(std::move(state));
        }
    }

    if (options.mode == bchaves::system::SearchMode::hybrid) {
         for (std::uint32_t i = 0; i < num_threads; ++i) {
            active_workers.fetch_add(1, std::memory_order_relaxed);
            workers.emplace_back([=, &matcher, &total_processed, &total_checks_processed, &found, &found_key, &found_mutex, &active_workers]() {
                struct WorkerExitGuard {
                    std::atomic<std::uint32_t>& counter;
                    ~WorkerExitGuard() { counter.fetch_sub(1, std::memory_order_relaxed); }
                } guard{active_workers};
                bchaves::system::pin_thread_to_core(i);
                run_hybrid_worker(i, start, end, matcher, options,
                                total_processed, total_checks_processed, found_key, found, found_mutex,
                                g_interrupt_requested);
            });
        }
    } else {
        for (std::uint32_t i = 0; i < num_threads; ++i) {
            active_workers.fetch_add(1, std::memory_order_relaxed);
            workers.emplace_back([=, &matcher, &total_processed, &found, &found_key, &found_mutex, &active_workers, &sequential_worker_states, &end]() {
                struct WorkerExitGuard {
                    std::atomic<std::uint32_t>& counter;
                    ~WorkerExitGuard() { counter.fetch_sub(1, std::memory_order_relaxed); }
                } guard{active_workers};
                bchaves::system::pin_thread_to_core(i);
                SequentialWorkerState& worker_state = *sequential_worker_states[i];
                bchaves::core::BigInt current;
                {
                    std::lock_guard<std::mutex> lock(worker_state.mutex);
                    current = worker_state.next_key;
                }
                if (current > end) {
                    return;
                }

                const bchaves::core::Secp256k1Point start_point = bchaves::core::secp256k1_multiply(current);
                bchaves::core::PointJacobian p_jac = bchaves::core::to_jacobian(start_point.x, start_point.y);
                bchaves::core::BigInt step_g_key(num_threads);
                bchaves::core::Secp256k1Point step_g = bchaves::core::secp256k1_multiply(step_g_key);

                bchaves::core::PointJacobian batch_p[kBatchSize];
                bchaves::core::Secp256k1Point batch_affine[kBatchSize];
                bchaves::core::BigInt batch_keys[kBatchSize];

                while (current <= end && !found.load()) {
                    if (g_interrupt_requested) break;
                    
                    bchaves::core::PointJacobian temp_p = p_jac;
                    bchaves::core::BigInt temp_key = current;
                    size_t valid_batch_size = 0;
                    for (; valid_batch_size < kBatchSize && temp_key <= end; ++valid_batch_size) {
                        batch_p[valid_batch_size] = temp_p;
                        batch_keys[valid_batch_size] = temp_key;
                        temp_p = bchaves::core::add_points_mixed(temp_p, step_g);
                        temp_key += step_g_key;
                    }
                    if (valid_batch_size == 0) {
                        break;
                    }

                    bchaves::core::batch_normalize(batch_p, batch_affine, valid_batch_size);

                    std::uint8_t batch_sha_out[8][32];
                    std::uint8_t batch_ripemd_out[8][20];
                    std::uint8_t* sha_ptr[8];
                    const std::uint8_t* sha_in_ptr[8];
                    std::uint8_t* ripemd_ptr[8];
                    const std::uint8_t* data_ptr[8];
                    alignas(32) std::uint8_t pub_bufs[8][65];

                    for (size_t k = 0; k < valid_batch_size; k += 8) {
                        const size_t lane_count = std::min<std::size_t>(8, valid_batch_size - k);
                        auto check_batch = [&](bool compress) {
                            size_t p_len = compress ? 33 : 65;
                            if (lane_count == 8) {
                                for (int u = 0; u < 8; ++u) {
                                    bchaves::core::serialize_pubkey(batch_affine[k + u], compress, pub_bufs[u]);
                                    data_ptr[u] = pub_bufs[u];
                                    sha_ptr[u] = batch_sha_out[u];
                                    sha_in_ptr[u] = batch_sha_out[u];
                                    ripemd_ptr[u] = batch_ripemd_out[u];
                                }

                                bchaves::core::Sha256::hash8(data_ptr, p_len, sha_ptr);
                                bchaves::core::ripemd160_batch8(sha_in_ptr, 32, ripemd_ptr);

                                for (int u = 0; u < 8; ++u) {
                                    uint64_t filter_hash;
                                    std::memcpy(&filter_hash, batch_ripemd_out[u], sizeof(uint64_t));
                                    if (matcher.filter && !matcher.filter->lookup(filter_hash)) continue;
                                    if (!matcher_contains_hash(matcher, batch_ripemd_out[u])) continue;

                                    std::lock_guard<std::mutex> lock(found_mutex);
                                    if (!found.load()) {
                                        bchaves::core::derive_key_info(batch_keys[k + u], found_key);
                                        found = true;
                                    }
                                }
                                return;
                            }

                            for (size_t u = 0; u < lane_count; ++u) {
                                bchaves::core::serialize_pubkey(batch_affine[k + u], compress, pub_bufs[u]);
                                const auto sha = bchaves::core::sha256(pub_bufs[u], p_len);
                                const auto ripemd = bchaves::core::ripemd160(sha.data(), sha.size());
                                uint64_t filter_hash;
                                std::memcpy(&filter_hash, ripemd.data(), sizeof(uint64_t));
                                if (matcher.filter && !matcher.filter->lookup(filter_hash)) continue;
                                if (!matcher_contains_hash(matcher, ripemd.data())) continue;

                                std::lock_guard<std::mutex> lock(found_mutex);
                                if (!found.load()) {
                                    bchaves::core::derive_key_info(batch_keys[k + u], found_key);
                                    found = true;
                                }
                            }
                        };

                        if (options.type == bchaves::system::SearchType::compress || options.type == bchaves::system::SearchType::both) {
                            check_batch(true);
                        }
                        if (options.type == bchaves::system::SearchType::uncompress || options.type == bchaves::system::SearchType::both) {
                            check_batch(false);
                        }

                        if (found.load(std::memory_order_relaxed)) break;
                    }
                    
                    total_processed += valid_batch_size;
                    p_jac = temp_p;
                    current = temp_key;
                    {
                        std::lock_guard<std::mutex> lock(worker_state.mutex);
                        worker_state.next_key = current;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(worker_state.mutex);
                    worker_state.next_key = current;
                }
            });
        }
    }

    auto last_stats = started;
    auto last_checkpoint = started;

    while (!found.load() && !g_interrupt_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_stats >= std::chrono::seconds(10)) {
            auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(now - started).count();
            const double key_rate = static_cast<double>(total_processed.load()) / std::max<double>(1.0, (double)elapsed_sec);
            const double check_rate = static_cast<double>(total_checks_processed.load()) / std::max<double>(1.0, (double)elapsed_sec);
            
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                const uint64_t total_c = g_hybrid_total_chunks;
                const double pct = total_c > 0 ? (100.0 * done / total_c) : 0.0;
                std::cout << "\r[*] Chunks: " << done << "/" << total_c
                          << " (" << std::fixed << std::setprecision(1) << pct << "%)"
                          << " | Keys/s: " << bchaves::system::format_rate(key_rate)
                          << " | Checks/s: " << bchaves::system::format_rate(check_rate) << "        " << std::flush;
            } else {
                std::cout << "\r[*] Processado: " << bchaves::system::format_key_count((double)total_processed.load()) << " | Speed: " << bchaves::system::format_rate(key_rate) << "        " << std::flush;
            }
            last_stats = now;
        }

        if (checkpoint_enabled && now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                checkpoint.algorithm = "address-hybrid";
                checkpoint.hybrid_chunk_counter = (done >= num_threads) ? (done - num_threads) : 0;
                checkpoint.hybrid_chunk_step = g_chunk_step;
                checkpoint.hybrid_chunk_size = g_hybrid_chunk_size;
                checkpoint.hybrid_total_chunks = g_hybrid_total_chunks;
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                const auto ckp_path = hybrid_checkpoint_path();
                bchaves::system::save_checkpoint(ckp_path, checkpoint, err);
            } else {
                checkpoint.algorithm = "address-sequential";
                checkpoint.progress_primary = total_processed.load();
                checkpoint.worker_currents = snapshot_worker_currents(sequential_worker_states);
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                if (bchaves::system::save_checkpoint(sequential_checkpoint_path, checkpoint, err)) {
                    // Checkpoint salvo silenciosamente
                }
            }
            last_checkpoint = now;
        }

        if (active_workers.load(std::memory_order_relaxed) == 0) break;
    }

    for (auto& t : workers) if (t.joinable()) t.join();
    std::cout << "\n";

    if (found.load()) {
        std::string ctx = "Address Search (bits:" + std::to_string(options.bits) + ")";
        bchaves::engine::report_found(found_key, ctx, persist_results);
        return 0;
    }

    if (g_interrupt_requested) {
        std::cout << "\n[!] Interrompido pelo usuário. Salvando estado final...\n";
        if (checkpoint_enabled) {
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                checkpoint.algorithm = "address-hybrid";
                checkpoint.hybrid_chunk_counter = (done >= num_threads) ? (done - num_threads) : 0;
                checkpoint.hybrid_chunk_step = g_chunk_step;
                checkpoint.hybrid_chunk_size = g_hybrid_chunk_size;
                checkpoint.hybrid_total_chunks = g_hybrid_total_chunks;
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                const auto ckp_path = hybrid_checkpoint_path();
                bchaves::system::save_checkpoint(ckp_path, checkpoint, err);
            } else {
                checkpoint.algorithm = "address-sequential";
                checkpoint.progress_primary = total_processed.load();
                checkpoint.worker_currents = snapshot_worker_currents(sequential_worker_states);
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                if (bchaves::system::save_checkpoint(sequential_checkpoint_path, checkpoint, err)) {
                    std::cout << "[+] Checkpoint de emergência salvo com sucesso.\n";
                } else {
                    std::cerr << "[E] Falha ao salvar checkpoint de emergência: " << err << "\n";
                }
            }
        }
    }
    return 0;
}

}  // namespace bchaves::engine
