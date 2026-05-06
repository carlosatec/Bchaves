---
last_mapped_date: 2026-05-02
---
# Tech Stack

## Languages & Runtime
- **C++17**: Primary language. Used for maximum performance and low-level hardware control.

## Build System
- **Make**: A custom `Makefile` is used to orchestrate builds. Compiles multiple binaries (`address`, `bsgs`, `kangaroo`, test executables).

## Compiler Flags & Optimization
- `-O3`, `-flto`: Aggressive optimization and Link-Time Optimization.
- Target Architecture Flags: `-march=native` (x86_64 defaults), `-march=armv8.2-a+crypto` (ARM64), `-march=westmere -msse2 -mno-avx` (Legacy x86).

## SIMD Intrinsics
The project makes heavy use of specialized SIMD instruction sets to maximize throughput:
- **AVX-512**: Used for high-end modern x86 servers.
- **AVX2 / AVX**: Used for standard modern x86 systems.
- **SSE4 / SSE2**: Fallback for legacy hardware.
- **NEON**: For ARM64 compatibility (Apple Silicon, AWS Graviton).

## Core Libraries (Internal)
- **Custom secp256k1**: The elliptic curve arithmetic is written from scratch, bypassing standard libraries like `libsecp256k1` for specialized search-oriented performance.
- **Custom Hashing**: In-house implementations of SHA-256 and RIPEMD-160, directly wired into the SIMD intrinsic paths.