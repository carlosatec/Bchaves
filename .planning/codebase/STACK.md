# Stack

Last mapped: 2026-05-06

## Language

- **C++17** (`-std=c++17`) — primary and only language
- No scripting or interpreted language layers

## Compiler & Build

- **g++** (GCC) via WSL2 on Windows
- Build system: **GNU Make** (`Makefile` at project root)
- Optimization flags: `-O3 -flto -Wall -Wextra`
- Architecture dispatch:
  - `ARCH=sse2` → `-march=westmere -msse2 -mno-avx`
  - `aarch64` → `-march=armv8.2-a+crypto`
  - Default → `-march=native`

## Runtime

- Pure native binary — no VM, no runtime dependencies
- Cross-platform: Linux (primary), Windows (via WSL)
- No external shared libraries required (fully statically compiled)

## Key Dependencies (All Header-Only / Vendored)

| Dependency | Type | Location |
|---|---|---|
| Secp256k1 arithmetic | Custom implementation | `core/secp256k1.cpp`, `core/secp256k1.hpp` |
| SHA-256 | Custom multi-ISA | `core/sha256-*.cpp`, `core/hash.cpp` |
| RIPEMD-160 | Custom multi-ISA | `core/ripemd160-*.cpp`, `core/ripemd160.hpp` |
| Base58Check | Custom | `core/base58.cpp` |
| Cuckoo Filter | Custom adaptive | `core/adaptive_filter.cpp`, `core/cuckoo.hpp` |

**Zero external package managers** — no `vcpkg`, `conan`, `apt`, or `npm`. All code is vendored.

## ISA-Specific Code

| ISA | Files | Features Used |
|---|---|---|
| SSE4.1 | `*-sse4.cpp`, `secp256k1-sse4.hpp` | `_mm_*` intrinsics |
| AVX2 | `*-avx2.cpp`, `secp256k1-avx2.hpp` | `_mm256_*` intrinsics |
| AVX-512 | `*-avx512.cpp`, `secp256k1-avx512.hpp` | `_mm512_*` intrinsics |
| NEON (ARM64) | `*-neon.cpp`, `secp256k1-arm64.hpp` | `vld1q_*`, ARMv8 Crypto Extensions |

## Configuration

- No config files — all configuration via CLI flags
- Hardware auto-detection at runtime (`system/hardware.cpp`)
- Auto-tune profiles: `safe`, `balanced`, `max`