---
phase: 06-01
plan: 01
subsystem: system, core
tags: [hardware, simd, optimization, auto-tune]
dependency_graph:
  requires: []
  provides: [HW-01, HW-02, HW-03, HW-04, HW-05, HW-06, AUTO-01, AUTO-02, AUTO-03, ISA-01, ISA-02, ISA-03]
  affects: [hash, secp256k1]
tech_stack:
  added: [SSE4.1/4.2, AVX, AVX2, AVX-512, NEON, ARM64 Crypto]
  patterns: [ISA-aware dispatch, CPUID detection, compile-time selection]
key_files:
  created:
    - core/sha256-sse4.cpp
    - core/sha256-avx.cpp
    - core/sha256-avx2.cpp
    - core/sha256-avx512.cpp
    - core/secp256k1-sse4.cpp
    - core/secp256k1-avx2.cpp
    - core/secp256k1-avx512.cpp
    - core/secp256k1-arm64.cpp
    - core/ripemd160-sse4.cpp
    - core/ripemd160-avx2.cpp
  modified:
    - system/hardware.cpp
    - system/hardware.hpp
    - system/types.hpp
    - system/cli.cpp
    - .planning/ROADMAP.md
decisions:
  - Use CPUID for x86 feature detection on both Windows and Linux
  - Use getauxval for ARM64 on Linux
  - ISA level determined at runtime: AVX512 > AVX2 > AVX > SSE4 > SSSE3 > NEON > Portable
metrics:
  duration: "~3 minutes"
  completed: "2026-05-01"
---

# Phase 6 Plan: Hardware Detection & Auto Mode Summary

## Overview
Enhanced hardware detection and auto mode for the Bitcoin performance engine with cross-platform support and ISA-aware optimization. This phase addresses gaps in CPU identification, cache detection, NUMA support, and adds SIMD-optimized crypto implementations.

## Tasks Completed

### Task 1: Enhanced Hardware Detection
| Item | Status |
|------|--------|
| CPU brand string (Windows Registry) | ✅ |
| CPU brand string (Linux /proc/cpuinfo) | ✅ |
| L1/L2 cache via CPUID Leaf 4 | ✅ |
| Physical/logical core detection | ✅ |
| NUMA detection (Windows/Linux) | ✅ |
| Memory channel estimation | ✅ |
| ARM64 NEON/SHA2/AES/PMULL | ✅ |
| SSSE3/SSE4/AVX detection | ✅ |

### Task 2: Per-ISA Code Organization
| File | ISA | Purpose |
|------|-----|---------|
| sha256-sse4.cpp | SSE4.1/4.2 SHA256 hash4 |
| sha256-avx.cpp | AVX 128-bit fallback |
| sha256-avx2.cpp | AVX2 256-bit hash8 |
| sha256-avx512.cpp | AVX-512 hash16 |
| secp256k1-sse4.cpp | Field ops SSE4.1 |
| secp256k1-avx2.cpp | Field ops AVX2 |
| secp256k1-avx512.cpp | Field ops AVX-512 |
| secp256k1-arm64.cpp | Field ops NEON |
| ripemd160-sse4.cpp | RIPEMD160 SSE4 |
| ripemd160-avx2.cpp | RIPEMD160 AVX2 |

### Task 3: ISA-Aware Auto-Tune
| Feature | Implementation |
|---------|--------------|
| Batch size scaling | SSSE3:1x, SSE4:2x, AVX:2x, AVX2:4x, AVX512:8x, NEON:2x |
| CPU-specific tuning | Xeon:2x, Core i7/i9:1.5x, Ryzen:1.5x, Apple:2x |
| Thread profiles | safe:phys/2, balanced:phys, max:logical |
| Table sizing | Based on available RAM (8-64GB scale) |

### Task 4: CLI Flag --list-hardware
Reports:
- CPU vendor, family, model
- Physical/logical cores, SMT status
- L1/L2/L3 cache sizes
- Memory (GB, channels, DDR generation)
- Detected features list
- ISA level (best available)
- NUMA status
- Recommended tuning values

## Deviations from Plan
- None - plan executed exactly as written

## Known Stubs
| Stub | File | Reason |
|------|------|--------|
| ARM64 Windows | system/hardware.cpp | getauxval Linux-only on ARM64 |
| DDR5 detection | system/hardware.cpp | Requires dmidecode, using heuristic |

## Threat Flags
| Flag | File | Description |
|------|------|-------------|
| None | - | No new network/auth surfaces introduced |

## Self-Check: PASSED
- All 4 tasks executed and committed
- 14 files changed (2 modified, 10 new, 2 docs)
- All ISA-specific files use #ifdef guards as specified
- --list-hardware flag fully implemented

## Commits
- 64e1924: feat(phase 6): enhanced hardware detection
- b2fb519: feat(phase 6): per-ISA code organization
- 5468b70: feat(phase 6): ISA-aware auto-tune and --list-hardware CLI flag
- 7dc9d0a: chore(phase 6): mark phase 6 as IN_PROGRESS in roadmap