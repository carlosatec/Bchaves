# Structure

Last mapped: 2026-05-06

## Directory Layout

```
Bchaves/
├── core/                          # Layer 4: Cryptographic primitives (32 files)
│   ├── secp256k1.cpp              # Elliptic curve arithmetic (39 KB)
│   ├── secp256k1.hpp              # Public interface for EC ops
│   ├── secp256k1_fleet.hpp        # SoA fleet layout for vectorized ops
│   ├── secp256k1_reduce.hpp       # Modular reduction constants
│   ├── secp256k1-sse4.hpp         # SSE4.1 point arithmetic kernel
│   ├── secp256k1-avx2.hpp         # AVX2 point arithmetic kernel
│   ├── secp256k1-avx512.hpp       # AVX-512 point arithmetic kernel
│   ├── secp256k1-arm64.hpp        # ARM64/NEON point arithmetic kernel
│   ├── hash.cpp                   # SHA-256 + RIPEMD-160 multi-ISA dispatch (21 KB)
│   ├── hash.hpp                   # Hash function interfaces
│   ├── sha256-sse4.cpp            # SHA-256 SSE4 kernel
│   ├── sha256-avx.cpp             # SHA-256 AVX kernel
│   ├── sha256-avx2.cpp            # SHA-256 AVX2 kernel
│   ├── sha256-avx512.cpp          # SHA-256 AVX-512 kernel
│   ├── sha256-arm64.cpp           # SHA-256 ARMv8 Crypto Extensions
│   ├── ripemd160.hpp              # RIPEMD-160 interface + scalar impl (16 KB)
│   ├── ripemd160-sse4.cpp         # RIPEMD-160 SSE4 kernel
│   ├── ripemd160-avx2.cpp         # RIPEMD-160 AVX2 kernel
│   ├── ripemd160-neon.cpp         # RIPEMD-160 NEON kernel
│   ├── simd_hashing.hpp           # SIMD hashing dispatch interface
│   ├── adaptive_filter.cpp        # AdaptiveCuckooFilter (runtime dispatch)
│   ├── adaptive_filter.hpp        # Cuckoo Filter public interface
│   ├── adaptive_filter_avx2.cpp   # AVX2 batch lookup kernel
│   ├── adaptive_filter_avx512.cpp # AVX-512 batch lookup kernel
│   ├── adaptive_filter_sse4.cpp   # SSE4 batch lookup kernel
│   ├── adaptive_filter_neon.cpp   # NEON batch lookup kernel
│   ├── cuckoo.hpp                 # Low-level Cuckoo hash primitives
│   ├── hash_table.hpp             # BSGS hash table
│   ├── bitcoin_format.cpp         # Bitcoin address formatting (WIF, Base58Check)
│   ├── bitcoin_format.hpp         # Formatting interface
│   ├── base58.cpp                 # Base58 encoding/decoding
│   └── base58.hpp                 # Base58 interface
│
├── engine/                        # Layer 2: Search algorithms (5 files)
│   ├── address.cpp                # Hybrid address search engine (44 KB)
│   ├── bsgs.cpp                   # Baby-step Giant-step engine (15 KB)
│   ├── kangaroo.cpp               # Pollard's Kangaroo engine (39 KB)
│   ├── app.cpp                    # Shared engine initialization
│   └── app.hpp                    # Engine interface declarations
│
├── system/                        # Layer 3: Platform abstraction (13 files)
│   ├── hardware.cpp               # CPU detection, affinity, NUMA (23 KB)
│   ├── hardware.hpp               # Hardware interface
│   ├── cli.cpp                    # CLI argument parsing (14 KB)
│   ├── cli.hpp                    # CLI interface
│   ├── checkpoint.cpp             # Binary checkpoint persistence + CRC32 (10 KB)
│   ├── checkpoint.hpp             # Checkpoint interface
│   ├── targets.cpp                # Target file parser (9 KB)
│   ├── targets.hpp                # Targets interface
│   ├── format.cpp                 # Telemetry formatting (5 KB)
│   ├── format.hpp                 # Format interface
│   ├── io.cpp                     # File I/O helpers (2 KB)
│   ├── io.hpp                     # I/O interface
│   └── types.hpp                  # Core type definitions (5 KB)
│
├── modulos/                       # Layer 1: Entry points (3 files)
│   ├── address.cpp                # main() for address search
│   ├── bsgs.cpp                   # main() for BSGS search
│   └── kangaroo.cpp               # main() for Kangaroo search
│
├── tests/                         # Test suite (5 files)
│   ├── crypto_test.cpp            # 73 unit tests (EC, hashing, encoding)
│   ├── simd_test.cpp              # SIMD kernel structural validation
│   ├── test_cuckoo_adaptive.cpp   # AdaptiveCuckooFilter test suite
│   ├── stress_test.cpp            # Signal handling stress test
│   └── test_crypto_arm64.cpp      # ARM64-specific crypto tests
│
├── puzzles/                       # Bitcoin puzzle target files
├── traps/                         # Kangaroo trap shard storage
├── build/                         # Compiled binaries (gitignored)
├── doc/                           # Documentation
├── scratch/                       # Temporary/experimental files
│
├── Makefile                       # GNU Make build system
├── README.md                      # Project documentation
├── CONTRIBUTING.md                # Contribution guidelines
├── LICENSE                        # MIT License
└── review.completo.md             # Full project performance review
```

## Naming Conventions

| Pattern | Example | Meaning |
|---|---|---|
| `*-sse4.cpp` | `sha256-sse4.cpp` | SSE4.1 SIMD kernel |
| `*-avx2.cpp` | `ripemd160-avx2.cpp` | AVX2 SIMD kernel |
| `*-avx512.cpp` | `sha256-avx512.cpp` | AVX-512 SIMD kernel |
| `*-arm64.cpp` | `sha256-arm64.cpp` | ARM64/NEON kernel |
| `*-neon.cpp` | `ripemd160-neon.cpp` | NEON kernel (alias for ARM64) |
| `secp256k1-*.hpp` | `secp256k1-avx2.hpp` | ISA-specific EC arithmetic |
| `*.hpp` | `hash.hpp` | C++ header (interface) |
| `*.cpp` | `hash.cpp` | C++ implementation |

## Build Targets

| Target | Command | Output |
|---|---|---|
| Address search | `make address` | `build/address` |
| BSGS search | `make bsgs` | `build/bsgs` |
| Kangaroo search | `make kangaroo` | `build/kangaroo` |
| All tests | `make test` | Runs all 4 test binaries |
| Clean | `make clean` | Removes `build/` |