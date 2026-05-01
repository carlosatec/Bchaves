# 🏗️ System Architecture

This document provides an in-depth overview of the **Bchaves** system architecture, from low-level cryptographic primitives to high-level search engine orchestration.

---

## 📐 High-Level Architecture

Bchaves follows a **three-layer architecture** designed for performance and modularity:

```
┌─────────────────────────────────────────────────────────────┐
│                     Search Modules (modulos/)               │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │
│   │   address    │  │   kangaroo  │  │      bsgs       │   │
│   └─────────────┘  └─────────────┘  └─────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Engine Layer (engine/)                 │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │
│   │  app.cpp    │  │ address.cpp │  │   matcher.cpp  │   │
│   └─────────────┘  └─────────────┘  └─────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Layer (core/)                     │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │
│   │ secp256k1   │  │    hash     │  │     cuckoo     │   │
│   └─────────────┘  └─────────────┘  └─────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    System Layer (system/)                   │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │
│   │   hardware  │  │     cli     │  │   checkpoint   │   │
│   └─────────────┘  └─────────────┘  └─────────────────┘   │
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

- **SHA-256**: Full implementation with AVX2 SIMD optimization (processes 8 hashes in parallel).
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
- **Backend Configuration**: Selects Secp256k1 arithmetic backend based on CPU features.

### Checkpoint System

- **Atomic Saves**: Progress saved to `.ckp` files after each batch.
- **Auto-Resume**: Detects existing checkpoints and resumes seamlessly.
- **Corruption Protection**: v6 checkpoint format with parameter validation.

---

## 🖥️ System Layer (`system/`)

Platform-specific utilities and runtime infrastructure.

### Hardware Detection (`system/hardware.cpp`)

- **CPU Feature Detection**: Identifies AVX2, BMI2, ARM Crypto extensions.
- **Auto-Tune Profiles**:
  - `safe`: 50% physical cores (notebooks)
  - `balanced`: 100% physical cores (servers)
  - `max`: 100% logical cores (dedicated rigs)

### CLI Parser (`system/cli.cpp`)

- **Unified Interface**: Consistent argument parsing across all search modules.
- **Validation**: Range checking and parameter validation.

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
                    │ (core/hash)             │
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
- **Cross-Platform**: Supports x86_64 (SSE2, AVX2) and ARM64 (Apple Silicon, AWS Graviton).

---

*Generated by GSD Documentation System.*
<!-- GSD-MARKER -->