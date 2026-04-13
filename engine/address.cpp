#include "engine/app.hpp"

#include "core/address.hpp"
#include "core/secp256k1.hpp"
#include "system/checkpoint.hpp"
#include "system/format.hpp"
#include "system/hardware.hpp"
#include "system/targets.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <iomanip> // Adicionado para setprecision

namespace bchaves::engine {
namespace {

volatile std::sig_atomic_t g_interrupt_requested = 0;
std::string g_g_checkpoint_path;

// Removido - agora em app.hpp

void handle_signal(int) {
    g_interrupt_requested = 1;
}

// ===== HYBRID: divisão BigInt exata =====
static uint64_t bigint_div_u64(const bchaves::core::BigInt& num, uint64_t denom) {
    if (denom == 0) return 0;
    __uint128_t rem = 0;
    uint64_t result[4] = {};
    for (int i = 3; i >= 0; --i) {
        const __uint128_t cur = (rem << 64) | num.limbs[i];
        result[i] = static_cast<uint64_t>(cur / denom);
        rem = cur % denom;
    }
    return result[0];
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

bool load_targets(const std::filesystem::path& path, AddressMatcher& matcher) {
    std::cout << "[+] Carregando: " << path.string() << '\n';
    auto result = bchaves::system::load_targets(path, false);
    if (result.entries.empty()) return false;
    
    for (const auto& entry : result.entries) {
        if (entry.payload.size() == 20) {
             std::array<std::uint8_t, 20> h;
             std::copy(entry.payload.begin(), entry.payload.end(), h.begin());
             matcher.hashes.push_back(h);
        }
    }
    std::cout << "[+] Alvos (Hash160): " << matcher.hashes.size() << '\n';
    return !matcher.hashes.empty();
}

bool check_hit(const AddressMatcher& matcher, const bchaves::core::DerivedKeyInfo& key_info) {
    for (const auto& target_hash : matcher.hashes) {
        if (key_info.address_payload_compressed.size() == 20) {
            if (std::memcmp(key_info.address_payload_compressed.data(), target_hash.data(), 20) == 0) return true;
        }
        if (key_info.address_payload_uncompressed.size() == 20) {
            if (std::memcmp(key_info.address_payload_uncompressed.data(), target_hash.data(), 20) == 0) return true;
        }
    }
    return false;
}

void print_found(const bchaves::core::DerivedKeyInfo& key_info,
               const std::chrono::steady_clock::duration& elapsed,
               std::uint32_t thread_id = 0) {
    const auto seconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
    std::cout << "[!] FOUND!\n";
    std::cout << "    Private Key: " << bchaves::core::bigint_to_hex(key_info.private_key) << '\n';
    std::cout << "    Address:   " << key_info.address_compressed << '\n';
    std::cout << "    Time:     " << bchaves::system::format_duration(seconds) << '\n';
}

void print_stats(uint64_t processed,
              const std::chrono::steady_clock::duration& elapsed,
              int mode,
              uint64_t forward = 0,
              uint64_t backward = 0) {
    const auto seconds = std::max<uint64_t>(1,
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()));
    const double rate = static_cast<double>(processed) / static_cast<double>(seconds);
    std::cout << "[+] Keys: " << bchaves::system::format_key_count(static_cast<double>(processed))
              << " | Rate: " << bchaves::system::format_rate(rate)
              << " | Time: " << bchaves::system::format_duration(seconds) << '\n';
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
    return false;
}

void run_hybrid_worker(
    std::uint32_t              tid,
    const bchaves::core::BigInt& range_start,
    const AddressMatcher&        matcher,
    const bchaves::system::AddressOptions& options,
    std::atomic<uint64_t>&       total_processed,
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

    bchaves::core::PointJacobian batch_p[kBatch];
    bchaves::core::Secp256k1Point batch_affine[kBatch];
    bchaves::core::BigInt batch_keys[kBatch];

    while (!interrupt && !found.load(std::memory_order_relaxed)) {
        const uint64_t my_idx = g_chunk_counter.fetch_add(1, std::memory_order_relaxed);
        if (my_idx >= g_hybrid_total_chunks) break;
        const uint64_t chunk_id = (my_idx * g_chunk_step) % g_hybrid_total_chunks;

        bchaves::core::BigInt cur_key = range_start;
        cur_key += bchaves::core::BigInt(chunk_id) * big_chunk;

        const auto base_pt = bchaves::core::secp256k1_multiply(cur_key);
        bchaves::core::PointJacobian p_jac = bchaves::core::to_jacobian(base_pt.x, base_pt.y);

        for (uint64_t done = 0;
             done < g_hybrid_chunk_size && !interrupt && !found.load(std::memory_order_relaxed);
             done += kBatch)
        {
            bchaves::core::PointJacobian tmp_jac = p_jac;
            bchaves::core::BigInt tmp_key = cur_key;
            for (size_t k = 0; k < kBatch; ++k) {
                batch_p[k]    = tmp_jac;
                batch_keys[k] = tmp_key;
                tmp_jac = bchaves::core::add_points_mixed(tmp_jac, G);
                ++tmp_key;
            }

            bchaves::core::batch_normalize(batch_p, batch_affine, kBatch);

            bool use_batch = bchaves::core::Sha256::supports_avx2();
            if (use_batch) {
                std::uint8_t batch_sha_out[8][32];
                std::uint8_t* sha_ptr[8];
                const std::uint8_t* data_ptr[8];
                std::uint8_t pub_bufs[8][65];

                for (size_t k = 0; k < kBatch; k += 8) {
                    for(int u=0; u<8; ++u) {
                        bchaves::core::serialize_pubkey(batch_affine[k+u], options.type != bchaves::system::SearchType::uncompress, pub_bufs[u]);
                        data_ptr[u] = pub_bufs[u];
                        sha_ptr[u] = batch_sha_out[u];
                    }
                    bchaves::core::Sha256::hash8(data_ptr, 33, sha_ptr); 

                    for(int u=0; u<8; ++u) {
                        const auto h160 = bchaves::core::ripemd160(batch_sha_out[u], 32);
                        for (const auto& target : matcher.hashes) {
                            if (std::memcmp(h160.data(), target.data(), 20) == 0) {
                                std::lock_guard<std::mutex> lock(found_mutex);
                                if (!found.load()) {
                                    bchaves::core::derive_key_info(batch_keys[k+u], found_key);
                                    found = true;
                                    if (!options.benchmark) {
                                        std::ofstream f("FOUND.txt", std::ios::app);
                                        f << "Private Key: " << bchaves::core::bigint_to_hex(batch_keys[k+u]) << "\n";
                                    }
                                }
                            }
                        }
                    }
                    if (found.load()) break;
                }
            } else {
                for (size_t k = 0; k < kBatch; ++k) {
                    std::uint8_t pub[65];
                    size_t p_len = bchaves::core::serialize_pubkey(batch_affine[k], options.type != bchaves::system::SearchType::uncompress, pub);
                    const auto sha  = bchaves::core::sha256(pub, p_len);
                    const auto h160 = bchaves::core::ripemd160(sha.data(), sha.size());

                    for (const auto& target : matcher.hashes) {
                        if (std::memcmp(h160.data(), target.data(), 20) == 0) {
                            std::lock_guard<std::mutex> lock(found_mutex);
                            if (!found.load()) {
                                bchaves::core::derive_key_info(batch_keys[k], found_key);
                                found = true;
                                if (!options.benchmark) {
                                    std::ofstream f("FOUND.txt", std::ios::app);
                                    f << "Private Key: " << bchaves::core::bigint_to_hex(batch_keys[k]) << "\n";
                                }
                            }
                        }
                    }
                    if (found.load()) break;
                }
            }

            p_jac = bchaves::core::add_points_mixed(p_jac, bG);
            cur_key += batch_step;
            total_processed.fetch_add(kBatch, std::memory_order_relaxed);
        }
    }
}

} // namespace

int run_address(const bchaves::system::AddressOptions& options) {
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

    std::signal(SIGINT, handle_signal);

    std::string backend_error;
    if (!bchaves::core::select_secp256k1_backend(bchaves::core::Secp256k1BackendKind::portable, backend_error)) {
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
    std::atomic<bool> found{false};
    bchaves::core::DerivedKeyInfo found_key;
    std::mutex found_mutex;

    // TODO: Dividir range entre threads e implementar worker loop P = P + G
    // Por enquanto, implementamos um despachante simples para validar o -t
    
    const std::string checkpoint_name = "address_" + std::to_string(options.bits) + "bit.ckp";
    bchaves::system::CheckpointState checkpoint{};
    checkpoint.algorithm = "address";
    checkpoint.range_start = bchaves::core::to_bytes32(start);
    checkpoint.range_end = bchaves::core::to_bytes32(end);

    if (options.mode == bchaves::system::SearchMode::hybrid) {
        static constexpr uint64_t kBatch = 1024;
        g_hybrid_chunk_size = static_cast<uint64_t>(kBatch) * options.chunk_k;
        if (g_hybrid_chunk_size < 1048576ULL) g_hybrid_chunk_size = 1048576ULL;

        const bchaves::core::BigInt diff = end - start;
        g_hybrid_total_chunks = bigint_div_u64(diff, g_hybrid_chunk_size);
        if (g_hybrid_total_chunks == 0) g_hybrid_total_chunks = 1;

        bool resuming = false;
        if (options.checkpoint_enabled) {
            const auto ckp_path = bchaves::system::default_checkpoint_path("address-hybrid", options.bits);
            if (std::filesystem::exists(ckp_path)) {
                std::string err;
                if (bchaves::system::load_checkpoint(ckp_path, checkpoint, err)
                    && checkpoint.algorithm == "address-hybrid"
                    && checkpoint.hybrid_chunk_size   == g_hybrid_chunk_size
                    && checkpoint.hybrid_total_chunks == g_hybrid_total_chunks) {
                    g_chunk_step = checkpoint.hybrid_chunk_step;
                    if (g_chunk_step == 0 || gcd64(g_chunk_step, g_hybrid_total_chunks) != 1)
                        g_chunk_step = find_coprime_step(g_hybrid_total_chunks);
                    g_chunk_counter.store(checkpoint.hybrid_chunk_counter);
                    resuming = true;
                    std::cout << "[+] Retomando: " << checkpoint.hybrid_chunk_counter
                              << "/" << g_hybrid_total_chunks << " chunks\n";
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
        if (options.checkpoint_enabled && std::filesystem::exists(checkpoint_name)) {
            std::string err;
            if (bchaves::system::load_checkpoint(checkpoint_name, checkpoint, err)) {
                total_processed = checkpoint.progress_primary;
                bchaves::core::BigInt prog(total_processed.load());
                start = start + prog;
                std::cout << "[+] Retomando de: " << bchaves::core::bigint_to_hex(start) << " (Progresso: " << total_processed.load() << ")\n";
            }
        }
    }

    std::vector<std::thread> workers;
    const size_t kBatchSize = tune.batch_size;

    if (options.mode == bchaves::system::SearchMode::hybrid) {
         for (std::uint32_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([=, &matcher, &total_processed, &found, &found_key, &found_mutex]() {
                run_hybrid_worker(i, start, matcher, options,
                                total_processed, found_key, found, found_mutex,
                                g_interrupt_requested);
            });
        }
    } else {
        for (std::uint32_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([=, &matcher, &total_processed, &found, &found_key, &found_mutex]() {
                bchaves::core::BigInt current = start;
                bchaves::core::BigInt thread_offset(i);
                bchaves::core::BigInt thread_step(num_threads);
                current += thread_offset * thread_step; 
                
                bchaves::core::PointJacobian p_jac = bchaves::core::to_jacobian(bchaves::core::secp256k1_multiply(current).x, bchaves::core::secp256k1_multiply(current).y);
                bchaves::core::BigInt step_g_key(num_threads);
                bchaves::core::BigInt step_g_batch_key(num_threads * kBatchSize);
                bchaves::core::Secp256k1Point step_g = bchaves::core::secp256k1_multiply(step_g_key);
                bchaves::core::Secp256k1Point step_g_batch = bchaves::core::secp256k1_multiply(step_g_batch_key);

                bchaves::core::PointJacobian batch_p[kBatchSize];
                bchaves::core::Secp256k1Point batch_affine[kBatchSize];
                bchaves::core::BigInt batch_keys[kBatchSize];

                while (current <= end && !found.load()) {
                    if (g_interrupt_requested) break;
                    
                    bchaves::core::PointJacobian temp_p = p_jac;
                    bchaves::core::BigInt temp_key = current;
                    for (size_t k = 0; k < kBatchSize; ++k) {
                        batch_p[k] = temp_p;
                        batch_keys[k] = temp_key;
                        temp_p = bchaves::core::add_points_mixed(temp_p, step_g);
                        temp_key += step_g_key;
                    }

                    bchaves::core::batch_normalize(batch_p, batch_affine, kBatchSize);

                    std::uint8_t batch_sha_out[8][32];
                    std::uint8_t* sha_ptr[8];
                    const std::uint8_t* data_ptr[8];
                    std::uint8_t pub_bufs[8][65];
                    bool use_batch = bchaves::core::Sha256::supports_avx2();

                    for (size_t k = 0; k < kBatchSize; k += 8) {
                        if (use_batch) {
                            for(int u=0; u<8; ++u) {
                                bchaves::core::serialize_pubkey(batch_affine[k+u], options.type != bchaves::system::SearchType::uncompress, pub_bufs[u]);
                                data_ptr[u] = pub_bufs[u];
                                sha_ptr[u] = batch_sha_out[u];
                            }
                            bchaves::core::Sha256::hash8(data_ptr, 33, sha_ptr); 

                            for(int u=0; u<8; ++u) {
                                const auto h160 = bchaves::core::ripemd160(batch_sha_out[u], 32);
                                for (const auto& target : matcher.hashes) {
                                    if (std::memcmp(h160.data(), target.data(), 20) == 0) {
                                        std::lock_guard<std::mutex> lock(found_mutex);
                                        if (!found.load()) {
                                            bchaves::core::derive_key_info(batch_keys[k+u], found_key);
                                            found = true;
                                            
                                            if (!options.benchmark) {
                                                std::ofstream f("FOUND.txt", std::ios::app);
                                                f << "Private Key (HEX): " << bchaves::core::bigint_to_hex(batch_keys[k+u]) << "\n";
                                                f << "Address (Comp):    " << found_key.address_compressed << "\n";
                                                f << "--------------------------------------------------------\n";
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            for(int u=0; u<8; ++u) {
                                std::uint8_t pub_buf[65];
                                size_t p_len = bchaves::core::serialize_pubkey(batch_affine[k+u], options.type != bchaves::system::SearchType::uncompress, pub_buf);
                                const auto sha = bchaves::core::sha256(pub_buf, p_len);
                                const auto h160 = bchaves::core::ripemd160(sha.data(), sha.size());
                                for (const auto& target : matcher.hashes) {
                                    if (std::memcmp(h160.data(), target.data(), 20) == 0) {
                                        std::lock_guard<std::mutex> lock(found_mutex);
                                        if (!found.load()) {
                                            bchaves::core::derive_key_info(batch_keys[k+u], found_key);
                                            found = true;
                                            if (!options.benchmark) {
                                                std::ofstream f("FOUND.txt", std::ios::app);
                                                f << "Private Key (HEX): " << bchaves::core::bigint_to_hex(batch_keys[k+u]) << "\n";
                                                f << "Address (Comp):    " << found_key.address_compressed << "\n";
                                                f << "--------------------------------------------------------\n";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (found.load()) break;
                    }
                    
                    total_processed += kBatchSize;
                    p_jac = bchaves::core::add_points_mixed(p_jac, step_g_batch);
                    current += step_g_batch_key;
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
            double rate = static_cast<double>(total_processed.load()) / std::max<double>(1.0, (double)elapsed_sec);
            
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                const uint64_t total_c = g_hybrid_total_chunks;
                const double pct = total_c > 0 ? (100.0 * done / total_c) : 0.0;
                std::cout << "\r[*] Chunks: " << done << "/" << total_c
                          << " (" << std::fixed << std::setprecision(1) << pct << "%)"
                          << " | Speed: " << bchaves::system::format_rate(rate) << "        " << std::flush;
            } else {
                std::cout << "\r[*] Processado: " << bchaves::system::format_key_count((double)total_processed.load()) << " | Speed: " << bchaves::system::format_rate(rate) << "        " << std::flush;
            }
            last_stats = now;
        }

        if (options.checkpoint_enabled && now - last_checkpoint >= std::chrono::seconds(options.checkpoint_interval_seconds)) {
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                checkpoint.algorithm = "address-hybrid";
                checkpoint.hybrid_chunk_counter = (done >= num_threads) ? (done - num_threads) : 0;
                checkpoint.hybrid_chunk_step = g_chunk_step;
                checkpoint.hybrid_chunk_size = g_hybrid_chunk_size;
                checkpoint.hybrid_total_chunks = g_hybrid_total_chunks;
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                const auto ckp_path = bchaves::system::default_checkpoint_path("address-hybrid", options.bits);
                bchaves::system::save_checkpoint(ckp_path, checkpoint, err);
            } else {
                checkpoint.progress_primary = total_processed.load();
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                if (bchaves::system::save_checkpoint(checkpoint_name, checkpoint, err)) {
                    // Checkpoint salvo silenciosamente
                }
            }
            last_checkpoint = now;
        }
        
        bool all_done = true;
        for(auto& t : workers) if(t.joinable()) { all_done = false; break; }
        if(all_done) break;
    }

    for (auto& t : workers) if (t.joinable()) t.join();
    std::cout << "\n";

    if (found.load()) {
        print_found(found_key, std::chrono::steady_clock::now() - started);
        return 0;
    }

    if (g_interrupt_requested) {
        std::cout << "\n[!] Interrompido pelo usuário. Salvando estado final...\n";
        if (options.checkpoint_enabled) {
            if (options.mode == bchaves::system::SearchMode::hybrid) {
                const uint64_t done = g_chunk_counter.load();
                checkpoint.algorithm = "address-hybrid";
                checkpoint.hybrid_chunk_counter = (done >= num_threads) ? (done - num_threads) : 0;
                checkpoint.hybrid_chunk_step = g_chunk_step;
                checkpoint.hybrid_chunk_size = g_hybrid_chunk_size;
                checkpoint.hybrid_total_chunks = g_hybrid_total_chunks;
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                const auto ckp_path = bchaves::system::default_checkpoint_path("address-hybrid", options.bits);
                bchaves::system::save_checkpoint(ckp_path, checkpoint, err);
            } else {
                checkpoint.progress_primary = total_processed.load();
                checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
                std::string err;
                if (bchaves::system::save_checkpoint(checkpoint_name, checkpoint, err)) {
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