# Technology Stack

**Analysis Date:** 2026-05-01

## Languages

**Primary:**
- C++17 - All core components (crypto, search engines, system utilities)

**Secondary:**
- None (pure C++ implementation)

## Runtime

**Environment:**
- Native binary (no runtime required)
- Platform: x86_64 and ARM64 (Apple Silicon, AWS Graviton)

**Build System:**
- Make (GNU Make)
- Compiler: g++ (GCC)
- Linker: GNU ld (via g++)

## Frameworks

**Core:**
- None - Pure C++17 custom implementation
- Custom Secp256k1 elliptic curve cryptography
- Custom SHA-256 and RIPEMD-160 hash implementations

**Testing:**
- Custom test runner via `crypto_test` executable
- No external test framework

**Build/Dev:**
- Makefile with multi-target builds
- Flags: `-std=c++17 -O3 -flto -Wall -Wextra`
- Architecture-specific optimization flags

## Key Dependencies

**Critical:**
- None (all cryptographic code is custom implementation)

**Infrastructure:**
- Standard C++17 library (`<array>`, `<vector>`, `<string>`, `<filesystem>`)
- POSIX APIs for threading (where available)

## Configuration

**Environment:**
- Command-line arguments only (no environment variable dependency)
- Configuration via CLI switches (see `system/cli.hpp`, `system/types.hpp`)

**Build:**
- `Makefile` - Primary build configuration
- No CMake, no package.json

## Platform Requirements

**Development:**
- g++ with C++17 support
- GNU Make
- POSIX-compliant shell (for thread pinning)

**Production:**
- x86_64 or ARM64 processor
- Linux, macOS, or Windows (via WSL/MSYS2)
- RAM: scales with target search space

---

*Stack analysis: 2026-05-01*