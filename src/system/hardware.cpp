#include "system/hardware.hpp"

#include <algorithm>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#endif

namespace qchaves::system {
namespace {

std::uint32_t detect_cpu_features() {
    std::uint32_t features = cpu_none;
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 1, 0);
    if (regs[2] & (1 << 19)) {
        features |= cpu_sse4;
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
        if (ecx & bit_SSE4_1) {
            features |= cpu_sse4;
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
#endif
    }
#endif
    return features;
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
    const unsigned int logical = std::thread::hardware_concurrency();
    info.num_cores = logical == 0 ? 1u : logical;
    info.num_physical_cores = std::max(1u, info.num_cores / 2u);
    info.ram_total = detect_total_ram();
    info.ram_available = detect_available_ram();
    info.features = detect_cpu_features();
    info.l1_cache = 32u * 1024u;
    info.l2_cache = 256u * 1024u;
    info.l3_cache = 0;
    info.is_numa = false;
    return info;
}

TuneProfile tune_for(const HardwareInfo& hardware, AutoTuneProfile profile, std::uint32_t requested_threads, std::uint32_t requested_table_k) {
    TuneProfile tune;
    switch (profile) {
        case AutoTuneProfile::safe:
            tune.threads = std::max(1u, hardware.num_physical_cores / 2u);
            tune.batch_size = 512u;
            break;
        case AutoTuneProfile::balanced:
            tune.threads = std::max(1u, hardware.num_physical_cores);
            tune.batch_size = 1024u;
            break;
        case AutoTuneProfile::max:
            tune.threads = std::max(1u, hardware.num_cores);
            tune.batch_size = 2048u;
            break;
    }
    if (requested_threads > 0) {
        tune.threads = requested_threads;
    }

    const std::uint64_t ram_gb = hardware.ram_available == 0 ? 0 : hardware.ram_available / (1024ull * 1024ull * 1024ull);
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

}  // namespace qchaves::system
