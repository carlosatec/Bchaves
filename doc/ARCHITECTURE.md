<!-- generated-by: gsd-doc-writer -->
# 🏗️ System Architecture

This document provides an in-depth overview of the **Bchaves** system architecture, from low-level cryptographic primitives to high-level search engine orchestration.

---

## 📐 High-Level Architecture

Bchaves follows a **four-layer architecture** designed for performance and modularity:

```
┌─────────────────────────────────────────────────────────────┐
│                     Search Modules (modulos/)                │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐     │
│   │   address    │  │   kangaroo  │  │      bsgs       │     │
│   └─────────────┘  └─────────────┘  └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Engine Layer (engine/)                 │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐     │
│   │  app.cpp    │  │ address.cpp │  │   matcher.cpp  │     │
│   └─────────────┘  └─────────────┘  └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Layer (core/)                      │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐     │
│   │ secp256k1   │  │    hash    │  │     cuckoo      │     │
│   └─────────────┘  └─────────────┘  └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    System Layer (system/)                    │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐     │
│   │   hardware  │  │     cli    │  │   checkpoint   │     │
│   └─────────────┘  └─────────────┘  └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

---

## 🧬 Core Layer (`core/`)

The core layer provides low-level cryptographic primitives with no external dependencies.

### Secp256k1 Elliptic Curve

Located in `core/secp256k1.cpp` and `core/secp256k1.hpp`:

- **BigInt**: 256-bit unsigned integer implementation using 4x64-bit limbs.
- **Point Operations**: Jacobian coordinates for efficient point addition and doubling.
- **GLV Endomorphism**: Implements the Gallant-Vanstone-Zyazin algorithm to split scalars into $k_1$ and $k_2$ components, achieving ~2x speedup in scalar multiplication.
- **Backend Selection**: Runtime selection between:
  - `auto`: Uses AVX2/ARM Crypto extensions when available
  - `portable`: Plain C++ implementation

### Hash Functions

Located in `core/hash.cpp` and `core/hash.hpp`:

- **SHA-256**: Full implementation with ISA-specific SIMD optimization.
- **Per-ISA Implementations**: The following specialized implementations are automatically selected based on CPU capabilities:
  - `core/sha256-sse4.cpp`: SSE4.1/4.2 optimized
  - `core/sha256-avx.cpp`: AVX (128-bit) optimized
  - `core/sha256-avx2.cpp`: AVX2 (256-bit) optimized - processes 8 hashes in parallel
  - `core/sha256-avx512.cpp`: AVX512 (512-bit) optimized
- **RIPEMD-160**: Bitcoin address hash composition.
- **Address Derivation**: P2PKH address generation from public keys (both compressed and uncompressed formats).

### Cuckoo Filter

Located in `core/cuckoo.hpp`:

- **Probabilistic Data Structure**: O(1) membership testing for target addresses.
- **Disk-Backed Storage**: Supports billions of entries with memory-mapped I/O.
- **False Positive Handling**: Configurable false positive rate for memory efficiency.

---

## ⚙️ Engine Layer (`engine/`)

The engine layer orchestrates search algorithms and target matching.

### Search Modules

| Module | Algorithm | Use Case |
|--------|-----------|----------|
| `address` | Hybrid partitioned explorer | Bitcoin Puzzle searches (bit-range) |
| `kangaroo` | Pollard's Kangaroo | Discrete logarithm in large ranges |
| `bsgs` | Baby-Step Giant-Step | Memory-optimized range searches |

### Address Matcher (`engine/app.cpp`)

- **Target Loading**: Parses `targets.txt` and populates the Cuckoo filter.
- **Result Reporting**: Writes found keys to `found.txt` with full context.
- **Backend Configuration**: Selects Secp256k1 arithmetic and SHA-256 hash backend based on CPU features.

### Checkpoint System

- **Atomic Saves**: Progress saved to `.ckp` files after each batch.
- **Auto-Resume**: Detects existing checkpoints and resumes seamlessly.
- **Corruption Protection**: v6 checkpoint format with parameter validation.

---

## 🖥️ System Layer (`system/`)

Platform-specific utilities and runtime infrastructure.

### Hardware Detection (`system/hardware.cpp`) - Phase 6 Enhanced

The hardware detection module provides comprehensive CPU and system profiling:

- **CPU Brand Detection**: Parses CPU vendor (Intel, AMD, ARM), family (Core i7, Ryzen 7, Cortex-A, Apple M1/M2/M3), and model number.
- **Feature Detection**: Identifies available SIMD instructions via CPUID:
  - x86: SSSE3, SSE4, AVX, AVX2, AVX512, SHA-NI, BMI2
  - ARM64: NEON, AES, SHA2, PMULL
- **Cache Detection**: Queries L1, L2, and L3 cache sizes using CPUID leaf 4.
- **Core Topology**: Detects physical cores, logical cores, and SMT/hyper-threading status.
- **NUMA Detection**: Identifies multi-node NUMA configurations on servers.
- **Memory Profiling**: Estimates total RAM, available RAM, memory channels, and DDR generation.
- **ISA Level Resolution**: Determines the highest available instruction set (SSSE3 → SSE4 → AVX → AVX2 → AVX512, or NEON for ARM).

### ISA-Aware Auto-Tune (`system/hardware.cpp`)

The auto-tune system automatically scales batch size based on detected ISA level:

| ISA Level | Base Batch | ISA Multiplier |
|-----------|------------|----------------|
| SSSE3 | 256 | 1x |
| SSE4 | 256 | 2x |
| AVX | 256 | 2x |
| AVX2 | 512 | 4x |
| AVX512 | 512 | 8x |
| NEON | 256 | 2x |

The batch size is further multiplied by the auto-tune profile (`safe`=1x, `balanced`=2x, `max`=4x) and CPU family adjustments (Xeon/Core i7/i9/Ryzen=1.5x, Apple Silicon=2x).

### CLI Parser (`system/cli.cpp`)

- **Unified Interface**: Consistent argument parsing across all search modules.
- **Validation**: Range checking and parameter validation.
- **Hardware Listing**: `--list-hardware` flag displays detected system capabilities.

### I/O and Checkpointing (`system/io.cpp`, `system/checkpoint.cpp`)

- **Memory-Mapped Files**: Efficient handling of large target sets.
- **Atomic Checkpoints**: Safe state persistence for long-running searches.

---

## 🧪 Data Flow

```
targets.txt
     │
     ▼
┌─────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ Load Targets│───▶│ Build Cuckoo    │───▶│ Initialize      │
│ (system/)   │    │ Filter (core/)  │    │ Search Engine    │
└─────────────┘    └─────────────────┘    └─────────────────┘
                                               │
                    ┌──────────────────────────┤
                    ▼                          ▼
           ┌────────────────┐      ┌─────────────────┐
           │ Generate Keys   │      │ ECC Multiply    │
           │ (modulos/)      │      │ (core/secp256k1)│
           └────────────────┘      └─────────────────┘
                    │                          │
                    └────────────┬─────────────┘
                                 ▼
                    ┌─────────────────────────┐
                    │ Hash (SHA256+RIPEMD160) │
                    │ (core/hash) - ISA-aware │
                    └─────────────────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │ Cuckoo Filter Match     │
                    │ (core/cuckoo)           │
                    └─────────────────────────┘
                                 │
                    ┌────────────┴────────────┐
                    ▼                         ▼
             ┌─────────────┐          ┌──────────────┐
             │ Not Found   │          │ FOUND!       │
             │ Continue    │          │ report_found │
             └─────────────┘          └──────────────┘
```

---

## 🔧 Build System

The project uses a **Makefile-based build system** with modular compilation:

- **Build Directory**: All binaries output to `build/`
- **Target Compilation**: Each search module (`address`, `bsgs`, `kangaroo`) compiles against shared `core/` and `engine/` sources.
- **Cross-Platform**: Supports x86_64 (SSE2, AVX, AVX2, AVX512) and ARM64 (Apple Silicon, AWS Graviton).
- **Build Targets**:
  - `make all` - Build all engines
  - `make address` - Build address engine only
  - `make bsgs` - Build BSGS engine only
  - `make kangaroo` - Build Kangaroo engine only
  - `make test` - Build and run cryptographic tests

---

*Generated by GSD Documentation System.*