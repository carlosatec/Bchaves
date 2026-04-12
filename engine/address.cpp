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
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <cstring>

namespace bchaves::engine {
namespace {

volatile std::sig_atomic_t g_interrupt_requested = 0;
std::string g_g_checkpoint_path;

// Removido - agora em app.hpp

void handle_signal(int) {
    g_interrupt_requested = 1;
}

bool load_targets(const std::filesystem::path& path, AddressMatcher& matcher) {
    std::cout << "[+] Carregando: " << path.string() << '\n';
    auto result = bchaves::system::load_targets(path, false);
    if (result.entries.empty()) return false;
    
    for (const auto& entry : result.entries) {
        // Para endereços BTC, extraímos o hash160 do payload (depois do byte de versão e antes do checksum)
        if (entry.payload.size() >= 21) {
             std::array<std::uint8_t, 20> h;
             std::copy(entry.payload.begin() + 1, entry.payload.begin() + 21, h.begin());
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
    if (options.bits > 0 && options.bits <= 256) {
        start = bchaves::core::BigInt(0);
        end = bchaves::core::BigInt(0);
        
        // start = 2^(bits-1)
        if (options.bits == 1) {
            start.limbs[0] = 1;
        } else {
            std::size_t bit_idx = options.bits - 1;
            start.limbs[bit_idx / 32] = (1u << (bit_idx % 32));
        }

        // end = 2^(bits) - 1
        for (std::uint32_t i = 0; i < options.bits; ++i) {
            end.limbs[i / 32] |= (1u << (i % 32));
        }
        
        std::cout << "[+] Bit range: " << options.bits << " bits\n";
        return true;
    }
    std::cerr << "[E] Use -b <bits> (1-256)\n";
    return false;
}

} // namespace

int run_address(const bchaves::system::AddressOptions& options) {
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

    std::uint32_t num_threads = options.threads;
    if (num_threads == 0) num_threads = bchaves::system::detect_hardware().num_cores;
    std::cout << "[+] Threads: " << num_threads << '\n';

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
    
    if (options.checkpoint_enabled && std::filesystem::exists(checkpoint_name)) {
        std::string err;
        if (bchaves::system::load_checkpoint(checkpoint_name, checkpoint, err)) {
            bchaves::core::BigInt saved_start;
            // No atual modelo, somamos o progresso ao range inicial
            total_processed = checkpoint.progress_primary;
            bchaves::core::BigInt prog(total_processed.load());
            start = start + prog;
            std::cout << "[+] Retomando de: " << bchaves::core::bigint_to_hex(start) << " (Progresso: " << total_processed.load() << ")\n";
        }
    }

    std::vector<std::thread> workers;
    for (std::uint32_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([&, i]() {
            bchaves::core::BigInt current = start;
            for(uint32_t s=0; s<i; ++s) ++current; 
            
            bchaves::core::Secp256k1Point p = bchaves::core::secp256k1_multiply(current);
            bchaves::core::Secp256k1Point g = bchaves::core::secp256k1_multiply(bchaves::core::BigInt(num_threads));
            
            while (current <= end && !found.load()) {
                if (g_interrupt_requested) break;
                
                const auto serialized = bchaves::core::serialize_pubkey(p, options.type != bchaves::system::SearchType::uncompress);
                const auto sha = bchaves::core::sha256(serialized.data(), serialized.size());
                const auto h160 = bchaves::core::ripemd160(sha.data(), sha.size());
                
                bool hit = false;
                for (const auto& target : matcher.hashes) {
                    if (std::memcmp(h160.data(), target.data(), 20) == 0) {
                        hit = true;
                        break;
                    }
                }

                if (hit) {
                    std::lock_guard<std::mutex> lock(found_mutex);
                    if (!found.load()) {
                        bchaves::core::derive_key_info(current, found_key);
                        found = true;
                        
                        // Salvar em FOUND.txt imediatamente
                        std::ofstream f("FOUND.txt", std::ios::app);
                        f << "Private Key (HEX): " << bchaves::core::bigint_to_hex(current) << "\n";
                        f << "Address (Comp):    " << found_key.address_compressed << "\n";
                        f << "Address (Uncomp):  " << found_key.address_uncompressed << "\n";
                        f << "WIF (Comp):        " << found_key.wif_compressed << "\n";
                        f << "--------------------------------------------------------\n";
                    }
                    break;
                }
                
                total_processed++;
                p = bchaves::core::secp256k1_add(p, g);
                for(uint32_t s=0; s<num_threads; ++s) ++current;
            }
        });
    }

    auto last_stats = started;
    auto last_checkpoint = started;

    while (!found.load() && !g_interrupt_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_stats >= std::chrono::seconds(10)) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - started).count();
            double rate = static_cast<double>(total_processed.load()) / std::max<double>(1.0, (double)elapsed);
            std::cout << "\r[*] Processado: " << total_processed.load() << " | Speed: " << bchaves::system::format_rate(rate) << "        " << std::flush;
            last_stats = now;
        }

        if (options.checkpoint_enabled && now - last_checkpoint >= std::chrono::seconds(60)) {
            checkpoint.progress_primary = total_processed.load();
            checkpoint.timestamp = static_cast<uint64_t>(std::time(nullptr));
            std::string err;
            if (bchaves::system::save_checkpoint(checkpoint_name, checkpoint, err)) {
                // Checkpoint salvo silenciosamente
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

    if (g_interrupt_requested) std::cout << "[!] Interrompido pelo usuário\n";
    return 0;
}

}  // namespace bchaves::engine