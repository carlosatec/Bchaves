/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Detecção de hardware e lógica de Auto-Tune (CPU, RAM, Cache).
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "system/hardware.hpp"

#include <algorithm>
#include <thread>
#include <fstream>
#include <sstream>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>
#include <processthreadsapi.h>
#include <winreg.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <sched.h>
#include <sys/stat.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#elif defined(__aarch64__) && defined(__linux__)
#include <sys/auxv.h>
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif
#ifndef HWCAP_SHA2
#define HWCAP_SHA2 (1 << 6)
#endif
#ifndef HWCAP_AES
#define HWCAP_AES (1 << 2)
#endif
#ifndef HWCAP_PMULL
#define HWCAP_PMULL (1 << 4)
#endif
#endif

namespace bchaves::system {
namespace {

// CPU feature bits for x86 (from CPUID)
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#ifndef bit_SSSE3
#define bit_SSSE3 (1 << 9)
#endif
#ifndef bit_SSE4_1
#define bit_SSE4_1 (1 << 19)
#endif
#ifndef bit_SSE4_2
#define bit_SSE4_2 (1 << 20)
#endif
#ifndef bit_AVX
#define bit_AVX (1 << 28)
#endif
#ifndef bit_AVX2
#define bit_AVX2 (1 << 5)
#endif
#ifndef bit_AVX512F
#define bit_AVX512F (1 << 16)
#endif
#ifndef bit_SHA
#define bit_SHA (1 << 29)
#endif
#ifndef bit_BMI2
#define bit_BMI2 (1 << 8)
#endif
#endif

std::string detect_cpu_brand() {
    std::string brand;
#if defined(_WIN32)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD size = sizeof(buffer);
        DWORD type;
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, &type,
            reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
            brand = buffer;
        }
        RegCloseKey(hKey);
    }
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0) {
                std::size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    brand = line.substr(colon + 1);
                    // Trim whitespace
                    std::size_t start = brand.find_first_not_of(" \t");
                    std::size_t end = brand.find_last_not_of(" \t");
                    if (start != std::string::npos) {
                        brand = brand.substr(start, end - start + 1);
                    }
                    break;
                }
            }
        }
    }
#endif
    return brand;
}

std::string parse_cpu_vendor(const std::string& brand) {
    std::string lower;
    lower.reserve(brand.size());
    for (char c : brand) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower.find("intel") != std::string::npos || lower.find("genuineintel") != std::string::npos) {
        return "GenuineIntel";
    }
    if (lower.find("amd") != std::string::npos || lower.find("authenticamd") != std::string::npos) {
        return "AuthenticAMD";
    }
    if (lower.find("arm") != std::string::npos) {
        return "ARM";
    }
    return "Unknown";
}

std::string parse_cpu_family(const std::string& brand) {
    std::string lower;
    lower.reserve(brand.size());
    for (char c : brand) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower.find("core i9") != std::string::npos) return "Core i9";
    if (lower.find("core i7") != std::string::npos) return "Core i7";
    if (lower.find("core i5") != std::string::npos) return "Core i5";
    if (lower.find("core i3") != std::string::npos) return "Core i3";
    if (lower.find("xeon") != std::string::npos) return "Xeon";
    if (lower.find("ryzen 7") != std::string::npos || lower.find("ryzen 9") != std::string::npos) return "Ryzen 7";
    if (lower.find("ryzen 5") != std::string::npos) return "Ryzen 5";
    if (lower.find("ryzen 3") != std::string::npos) return "Ryzen 3";
    if (lower.find("cortex-a") != std::string::npos) {
        // Extract A7x or A5x
        std::size_t pos = lower.find("cortex-a");
        if (pos != std::string::npos && pos + 9 < lower.size()) {
            std::string num = lower.substr(pos + 9, 2);
            if (std::isdigit(static_cast<unsigned char>(num[0]))) {
                return "Cortex-A" + num;
            }
        }
        return "Cortex-A";
    }
    if (lower.find("apple") != std::string::npos) {
        if (lower.find("m1 pro") != std::string::npos) return "Apple M1 Pro";
        if (lower.find("m1 max") != std::string::npos) return "Apple M1 Max";
        if (lower.find("m1") != std::string::npos) return "Apple M1";
        if (lower.find("m2") != std::string::npos) return "Apple M2";
        if (lower.find("m3") != std::string::npos) return "Apple M3";
        return "Apple Silicon";
    }
    return "Unknown";
}

std::string parse_cpu_model(const std::string& brand) {
    // Extract model number (e.g., 9700K, 5800X3D, 12700K)
    std::string result;
    bool capturing = false;
    for (std::size_t i = 0; i < brand.size(); ++i) {
        char c = brand[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            capturing = true;
            result.push_back(c);
        } else if (capturing && (c == 'K' || c == 'X' || c == 'G' || c == 'H' || c == 'U')) {
            result.push_back(c);
        } else if (capturing && !std::isdigit(static_cast<unsigned char>(c))) {
            if (result.size() >= 2) break;
            result.clear();
            capturing = false;
        }
    }
    return result.empty() ? "Unknown" : result;
}

std::uint32_t detect_cpu_features() {
    std::uint32_t features = cpu_none;
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 1, 0);
    if (regs[2] & (1 << 9)) {
        features |= cpu_ssse3;
    }
    if (regs[2] & (1 << 19)) {
        features |= cpu_sse4;
    }
    if (regs[2] & (1 << 28)) {
        features |= cpu_avx;
    }
    __cpuidex(regs, 7, 0);
    if (regs[1] & (1 << 5)) {
        features |= cpu_avx2;
    }
    if (regs[1] & (1 << 16)) {
        features |= cpu_avx512;
    }
    if (regs[1] & (1 << 29)) {
        features |= cpu_sha_ni;
    }
    if (regs[1] & (1 << 8)) {
        features |= cpu_bmi2;
    }
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (ecx & bit_SSSE3) {
            features |= cpu_ssse3;
        }
        if (ecx & bit_SSE4_1) {
            features |= cpu_sse4;
        }
        if (ecx & bit_AVX) {
            features |= cpu_avx;
        }
    }
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & bit_AVX2) {
            features |= cpu_avx2;
        }
        if (ebx & bit_AVX512F) {
            features |= cpu_avx512;
        }
#ifdef bit_SHA
        if (ebx & bit_SHA) {
            features |= cpu_sha_ni;
        }
#endif
#ifdef bit_BMI2
        if (ebx & bit_BMI2) {
            features |= cpu_bmi2;
        }
    }
#elif defined(__aarch64__) && defined(__linux__)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_ASIMD) {
        features |= cpu_neon;
    }
    if (hwcap & HWCAP_SHA2) {
        features |= cpu_sha_ni;
    }
    if (hwcap & HWCAP_AES) {
        features |= cpu_aes;
    }
    if (hwcap & HWCAP_PMULL) {
        features |= cpu_pmull;
    }
#endif
    return features;
}

std::string detect_isa_level(std::uint32_t features) {
    if (features & cpu_avx512) return "AVX512";
    if (features & cpu_avx2) return "AVX2";
    if (features & cpu_avx) return "AVX";
    if (features & cpu_sse4) return "SSE4";
    if (features & cpu_ssse3) return "SSSE3";
    if (features & cpu_neon) return "NEON";
    return "None";
}

std::pair<std::uint32_t, std::uint32_t> detect_cache_sizes() {
    std::uint32_t l1 = 32 * 1024;  // Default 32KB
    std::uint32_t l2 = 256 * 1024; // Default 256KB
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned int eax, ebx, ecx, edx;
    // CPUID Leaf 4: Deterministic Cache Parameters
    // EAX=4, ECX=0 for L1 data cache
    if (__get_cpuid_count(4, 0, &eax, &ebx, &ecx, &edx)) {
        if ((eax & 0x1F) == 1) {  // L1 data cache
            unsigned int ways = ((ebx >> 22) & 0x3FF) + 1;
            unsigned int partitions = ((ebx >> 12) & 0x3FF) + 1;
            unsigned int line_size = (ebx & 0xFFF) + 1;
            unsigned int sets = ecx + 1;
            l1 = ways * partitions * line_size * sets;
        }
    }
    // CPUID Leaf 4, ECX=1 for L2 cache
    if (__get_cpuid_count(4, 1, &eax, &ebx, &ecx, &edx)) {
        if ((eax & 0x1F) == 2) {  // L2 cache
            unsigned int ways = ((ebx >> 22) & 0x3FF) + 1;
            unsigned int partitions = ((ebx >> 12) & 0x3FF) + 1;
            unsigned int line_size = (ebx & 0xFFF) + 1;
            unsigned int sets = ecx + 1;
            l2 = ways * partitions * line_size * sets;
        }
    }
#endif
    return {l1, l2};
}

std::uint32_t detect_l3_cache() {
    std::uint32_t l3 = 8 * 1024 * 1024; // Default 8MB
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned int eax, ebx, ecx, edx;
    // CPUID Leaf 4, ECX=3 for L3 cache
    if (__get_cpuid_count(4, 3, &eax, &ebx, &ecx, &edx)) {
        if ((eax & 0x1F) == 3) {  // L3 cache
            unsigned int ways = ((ebx >> 22) & 0x3FF) + 1;
            unsigned int partitions = ((ebx >> 12) & 0x3FF) + 1;
            unsigned int line_size = (ebx & 0xFFF) + 1;
            unsigned int sets = ecx + 1;
            l3 = ways * partitions * line_size * sets;
        }
    }
#endif
    return l3;
}

std::uint32_t detect_physical_cores() {
    std::uint32_t physical = 1;
    std::uint32_t logical = std::thread::hardware_concurrency();
#if defined(_WIN32)
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_INFORMATION_RELATIONSHIP::RelationProcessorCore, nullptr, &bufferSize);
    if (bufferSize > 0) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* buffer =
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(new char[bufferSize]);
        if (GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_INFORMATION_RELATIONSHIP::RelationProcessorCore,
            buffer, &bufferSize)) {
            std::uint32_t count = 0;
            std::size_t offset = 0;
            while (offset < bufferSize) {
                SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* info =
                    reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                        reinterpret_cast<char*>(buffer) + offset);
                if (info->Relationship == LOGICAL_PROCESSOR_INFORMATION_RELATIONSHIP::RelationProcessorCore) {
                    count++;
                }
                offset += info->Size;
            }
            physical = count > 0 ? count : logical;
        }
        delete[] reinterpret_cast<char*>(buffer);
    }
#elif defined(__linux__)
    // Read from /proc/cpuinfo to get physical id mapping
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        std::uint32_t max_physical_id = 0;
        std::uint32_t max_core_id = 0;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("physical id", 0) == 0) {
                std::size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string val = line.substr(colon + 1);
                    std::uint32_t id = static_cast<std::uint32_t>(std::stoul(val));
                    max_physical_id = std::max(max_physical_id, id);
                }
            }
            if (line.rfind("core id", 0) == 0) {
                std::size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string val = line.substr(colon + 1);
                    std::uint32_t id = static_cast<std::uint32_t>(std::stoul(val));
                    max_core_id = std::max(max_core_id, id);
                }
            }
        }
        if (max_physical_id > 0) {
            physical = max_physical_id + 1;
        } else if (max_core_id > 0) {
            physical = max_core_id + 1;
        } else {
            physical = logical;
        }
    }
#else
    physical = logical;
#endif
    // On ARM, physical = logical (no SMT)
#if defined(__aarch64__)
    physical = logical;
#endif
    return std::max(1u, physical);
}

bool detect_smt_enabled() {
#if defined(__aarch64__)
    return false;
#elif defined(_WIN32) || defined(__linux__)
    std::uint32_t logical = std::thread::hardware_concurrency();
    std::uint32_t physical = detect_physical_cores();
    return logical > physical;
#else
    return false;
#endif
}

std::uint32_t detect_numa_nodes() {
    std::uint32_t nodes = 1;
#if defined(_WIN32)
    ULONG highestNode;
    if (GetNumaHighestNodeNumber(&highestNode)) {
        nodes = highestNode + 1;
    }
#elif defined(__linux__)
    // Check /sys/devices/system/node/
    struct stat st;
    for (char c = '0'; c <= '9'; ++c) {
        std::string path = "/sys/devices/system/node/node";
        path += c;
        if (stat(path.c_str(), &st) == 0) {
            nodes = c - '0' + 1;
        } else {
            break;
        }
    }
#endif
    return nodes;
}

std::uint32_t detect_memory_channels() {
    std::uint32_t channels = 2; // Default
#if defined(_WIN32)
    // WMI query for memory channels (simplified)
    channels = 2;
#elif defined(__linux__)
    // Read from dmidecode if available
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") != std::string::npos) {
                // Estimate channels from total memory
                std::size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string val = line.substr(colon + 1);
                    std::uint64_t kb = static_cast<std::uint64_t>(std::stoul(val));
                    std::uint64_t gb = kb / (1024 * 1024);
                    if (gb >= 32) channels = 4;
                    else if (gb >= 16) channels = 2;
                    else channels = 1;
                }
                break;
            }
        }
    }
#endif
    return channels;
}

std::uint32_t detect_memory_gen() {
    std::uint32_t gen = 4; // Default DDR4
#if defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") != std::string::npos) {
                // Check for DDR5 indicator in dmesg or elsewhere
                // For now, default to DDR4, could enhanced with dmidecode
                gen = 4;
                break;
            }
        }
    }
#endif
    return gen;
}

std::uint64_t detect_total_ram() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        return static_cast<std::uint64_t>(status.ullTotalPhys);
    }
#elif defined(__linux__)
    struct sysinfo info {};
    if (sysinfo(&info) == 0) {
        return static_cast<std::uint64_t>(info.totalram) * info.mem_unit;
    }
#endif
    return 0;
}

std::uint64_t detect_available_ram() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        return static_cast<std::uint64_t>(status.ullAvailPhys);
    }
#elif defined(__linux__)
    struct sysinfo info {};
    if (sysinfo(&info) == 0) {
        return static_cast<std::uint64_t>(info.freeram) * info.mem_unit;
    }
#endif
    return 0;
}

}  // namespace

HardwareInfo detect_hardware() {
    HardwareInfo info;
    
    // Detect CPU brand and parse
    info.cpu_vendor = parse_cpu_vendor(detect_cpu_brand());
    std::string brand = detect_cpu_brand();
    info.cpu_family = parse_cpu_family(brand);
    info.cpu_model = parse_cpu_model(brand);
    
    // Core counts
    info.num_logical_cores = std::thread::hardware_concurrency();
    if (info.num_logical_cores == 0) info.num_logical_cores = 1;
    info.num_physical_cores = detect_physical_cores();
    info.num_cores = info.num_logical_cores;
    info.is_smt_enabled = detect_smt_enabled();
    
    // Memory
    info.ram_total = detect_total_ram();
    info.ram_available = detect_available_ram();
    info.memory_channels = detect_memory_channels();
    info.memory_gen = detect_memory_gen();
    
    // CPU features
    info.features = detect_cpu_features();
    info.isa_level = detect_isa_level(info.features);
    
    // Cache sizes
    auto cache = detect_cache_sizes();
    info.l1_cache = cache.first;
    info.l2_cache = cache.second;
    info.l3_cache = detect_l3_cache();
    
    // NUMA
    info.is_numa = detect_numa_nodes() > 1;
    
    return info;
}

TuneProfile tune_for(const HardwareInfo& hardware, AutoTuneProfile profile, std::uint32_t requested_threads, std::uint32_t requested_table_k) {
    TuneProfile tune;
    
    // Base batch size by ISA level
    std::uint32_t base_batch = 256u;
    std::uint32_t isa_multiplier = 1u;
    
    if (hardware.features & cpu_ssse3) {
        base_batch = 256u;
        isa_multiplier = 1u;
    }
    if (hardware.features & cpu_sse4) {
        base_batch = 256u;
        isa_multiplier = 2u;
    }
    if (hardware.features & cpu_avx) {
        base_batch = 256u;
        isa_multiplier = 2u;
    }
    if (hardware.features & cpu_avx2) {
        base_batch = 512u;
        isa_multiplier = 4u;
    }
    if (hardware.features & cpu_avx512) {
        base_batch = 512u;
        isa_multiplier = 8u;
    }
    if (hardware.features & cpu_neon) {
        base_batch = 256u;
        isa_multiplier = 2u;
    }
    
    // Scale by L3 cache
    if (hardware.l3_cache >= (8u * 1024u * 1024u)) {
        base_batch = std::max(base_batch, 1024u);
    }
    
    // CPU-specific tuning adjustments
    float cpu_multiplier = 1.0f;
    if (hardware.cpu_family.find("Xeon") != std::string::npos) {
        cpu_multiplier = 2.0f;
    } else if (hardware.cpu_family.find("Core i7") != std::string::npos ||
               hardware.cpu_family.find("Core i9") != std::string::npos) {
        cpu_multiplier = 1.5f;
    } else if (hardware.cpu_family.find("Ryzen") != std::string::npos) {
        cpu_multiplier = 1.5f;
    } else if (hardware.cpu_family.find("Apple") != std::string::npos) {
        cpu_multiplier = 2.0f;
    }
    
    // Apply profile
    switch (profile) {
        case AutoTuneProfile::safe:
            tune.threads = std::max(1u, hardware.num_physical_cores / 2u);
            if (hardware.is_smt_enabled && hardware.cpu_family.find("AMD") != std::string::npos) {
                tune.threads = std::max(1u, hardware.num_physical_cores / 4u);
            }
            tune.batch_size = base_batch * isa_multiplier;
            break;
        case AutoTuneProfile::balanced:
            tune.threads = std::max(1u, hardware.num_physical_cores);
            tune.batch_size = base_batch * isa_multiplier * 2u;
            break;
        case AutoTuneProfile::max:
            tune.threads = std::max(1u, hardware.num_logical_cores);
            tune.batch_size = base_batch * isa_multiplier * 4u;
            break;
    }
    
    // Apply CPU-specific multiplier
    tune.batch_size = static_cast<std::uint32_t>(tune.batch_size * cpu_multiplier);
    
    if (requested_threads > 0) {
        tune.threads = requested_threads;
    }
    
    // Table size based on available RAM
    const std::uint64_t ram_gb = hardware.ram_available == 0 ? 0 : 
        hardware.ram_available / (1024ull * 1024ull * 1024ull);
    if (requested_table_k > 0) {
        tune.table_k = requested_table_k;
    } else if (ram_gb >= 64) {
        tune.table_k = 8192u;
    } else if (ram_gb >= 32) {
        tune.table_k = 4096u;
    } else if (ram_gb >= 16) {
        tune.table_k = 2048u;
    } else if (ram_gb >= 8) {
        tune.table_k = 1024u;
    } else {
        tune.table_k = 512u;
    }
    
    return tune;
}

bool pin_thread_to_core(std::uint32_t core_id) {
#if defined(_WIN32)
    DWORD_PTR mask = 1ULL << core_id;
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false;
#endif
}

void pin_all_threads(std::uint32_t num_threads) {
    for (std::uint32_t i = 0; i < num_threads; ++i) {
        pin_thread_to_core(i);
    }
}

}  // namespace bchaves::system