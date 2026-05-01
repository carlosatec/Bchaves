# Architecture
**Analysis Date:** 2026-05-01
## System Overview
```text
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│         `modulos/*.cpp` - CLI entry points                 │
├──────────────────┬──────────────────┬───────────────────────┤
│   `modulos/      │   `modulos/      │    `modulos/          │
│    address.cpp`   │    bsgs.cpp`     │     kangaroo.cpp`     │
└────────┬─────────┴────────┬─────────┴──────────┬────────────┘
         │                  │                     │
         ▼                  ▼                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Engine Layer (`engine/`)                      │
│  `engine/address.cpp`, `engine/bsgs.cpp`, `engine/kangaroo.cpp`│
│  `engine/app.cpp` - Orchestration & reporting               │
├──────────────────┬──────────────────┬───────────────────────┤
│   `engine/       │   `engine/       │    `engine/          │
│    address.cpp`  │    bsgs.cpp`     │     kangaroo.cpp`      │
│  Search Logic    │  BSGS Logic      │   Kangaroo Logic      │
└────────┬─────────┴────────┬─────────┴──────────┬────────────┘
         │                  │                     │
         ▼                  ▼                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Core Layer (`core/`)                         │
│  `core/secp256k1.cpp` - Elliptic curve math                │
│  `core/address.cpp` - Bitcoin address derivation          │
│  `core/hash.cpp` - SHA-256, RIPEMD-160                    │
│  `core/base58.cpp` - Base58 encoding                      │
└────────┬─────────┴────────┬─────────┴──────────┬────────────┘
         │                  │                     │
         ▼                  ▼                     ▼
┌─────────────────────────────────────────────────────────────┐
│              System Layer (`system/`)                     │
│  `system/cli.cpp` - Command-line parsing                    │
│  `system/io.cpp` - File I/O, result persistence            │
│  `system/checkpoint.cpp` - State checkpointing             │
│  `system/hardware.cpp` - CPU detection, thread affinity   │
│  `system/format.cpp` - Output formatting                  │
│  `system/targets.cpp` - Target file loading               │
│  `system/types.hpp` - Type definitions                    │
���─────────────────────────────────────────────────────────────┘
```
## Component Responsibilities
| Component | Responsibility | File |
|-----------|----------------|------|
| CLI Entry | Parse args, dispatch to engine | `modulos/*.cpp` |
| Address Search | Hybrid/sequential key search | `engine/address.cpp` |
| BSGS Search | Baby-step giant-step | `engine/bsgs.cpp` |
| Kangaroo | Pollard's kangaroo algorithm | `engine/kangaroo.cpp` |
| Orchestration | Backend config, reporting | `engine/app.cpp` |
| Secp256k1 Math | Point multiplication, GLV | `core/secp256k1.cpp` |
| Address Derivation | Private key → BTC address | `core/address.cpp` |
| Hashing | SHA-256, RIPEMD-160 | `core/hash.cpp` |
| Base58 | Base58Check encoding | `core/base58.cpp` |
| Checkpoint | Progress persistence | `system/checkpoint.cpp` |
| Hardware | CPU detection, affinity | `system/hardware.cpp` |
## Pattern Overview
**Overall:** Layered modular monolith with pluggable search algorithms
**Key Characteristics:**
- Namespaced C++ (`bchaves::core`, `bchaves::engine`, `bchaves::system`)
- Header-only support for simple types (`*.hpp`)
- Separate compilation for logic (`*.cpp`)
- Three-layer architecture: Modulos → Engine → Core/System
## Layers
**Modulos Layer:**
- Purpose: CLI entry points (main functions)
- Location: `modulos/`
- Contains: `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`
- Depends on: `engine/app.hpp`, `system/cli.hpp`
- Used by: Executable binaries
**Engine Layer:**
- Purpose: Search algorithm implementations
- Location: `engine/`
- Contains: `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`, `app.cpp`
- Depends on: `core/`, `system/`
- Used by: `modulos/`
**Core Layer:**
- Purpose: Cryptographic primitives
- Location: `core/`
- Contains: `secp256k1.cpp`, `address.cpp`, `hash.cpp`, `base58.cpp`
- Depends on: Standard library only
- Used by: `engine/`
**System Layer:**
- Purpose: Infrastructure services
- Location: `system/`
- Contains: `cli.cpp`, `io.cpp`, `checkpoint.cpp`, `hardware.cpp`, etc.
- Depends on: Standard library only
- Used by: `engine/`, `modulos/`
## Data Flow
### Address Search Flow
1. **CLI Parse** (`system/cli.cpp:19`) - Parse `-b`, `-k`, `-R` options
2. **Hardware Detect** (`system/hardware.cpp`) - Detect cores, cache, features
3. **Targets Load** (`system/targets.cpp`) - Load address targets, build matcher
4. **Range Resolve** (`engine/address.cpp:134`) - Calculate bit range bounds
5. **Workers Spawn** (`engine/address.cpp:638-777`) - Spawn threads
6. **Key Generation** - Generate private keys (sequential or hybrid LCG)
7. **Point Multiply** (`core/secp256k1.cpp:847`) - Compute public keys
8. **Batch Hash** (`core/hash.cpp`) - SHA-256 + RIPEMD-160 batched
9. **Matcher Check** (`engine/address.cpp:105`) - CuckooFilter + binary search
10. **Match Found** - Report and persist result (`system/io.cpp`)
### Key Checkpointing Flow
1. **Checkpoint Save** (`system/checkpoint.cpp`) - Periodic state serialization
2. **Chunk State** - For hybrid mode: counter, step, size
3. **Worker State** - For sequential mode: per-thread current key
4. **Resume Load** (`engine/address.cpp:550-588`) - Load checkpoint, verify compatibility
## Key Abstractions
**BigInt:**
- Purpose: 256-bit integer for private keys
- Location: `core/secp256k1.cpp:26-37`
- Pattern: 4 × 64-bit limbs, little-endian
**Secp256k1Point:**
- Purpose: Elliptic curve point representation
- Location: `core/secp256k1.cpp:56-60`
- Pattern: Affine coordinates (x, y) + infinity flag
**PointJacobian:**
- Purpose: Jacobian projective coordinates for efficient arithmetic
- Location: `core/secp256k1.cpp:62-66`
- Pattern: (x/z², y/z³, z)
**AddressMatcher:**
- Purpose: Target address matching with O(1) filter
- Location: `engine/app.cpp:26-29`
- Pattern: Sorted vector + CuckooFilter
**CheckpointState:**
- Purpose: Serializable search progress
- Location: `system/types.hpp:135-156`
- Pattern: Versioned struct with optional hybrid/worker fields
## Entry Points
**address:**
- Location: `modulos/address.cpp`
- Triggers: `./build/address targets.txt -b 71`
- Responsibilities: Parse CLI, spawn address search workers
**bsgs:**
- Location: `modulos/bsgs.cpp`
- Triggers: `./build/bsgs targets.txt -b 40`
- Responsibilities: BSGS table build and lookup
**kangaroo:**
- Location: `modulos/kangaroo.cpp`
- Triggers: `./build/kangaroo targets.txt -b 75`
- Responsibilities: Kangaroo discrete log search
**crypto_test:**
- Location: `tests/crypto_test.cpp`
- Triggers: `make test`
- Responsibilities: Cryptographic integrity validation
## Architectural Constraints
- **Threading:** `std::thread` with manual pinning via `system/hardware.cpp:pin_thread_to_core()`
- **Global state:** `g_interrupt_requested` (`engine/address.cpp:38`), `g_chunk_counter` (`engine/address.cpp:72`)
- **No external dependencies:** Pure C++17, no third-party crypto libraries
- **Memory:** Stack-allocated batch buffers (`alignas(32)`) in hot loops
## Anti-Patterns
### TODO Comment - Incomplete Assembly
**What happens:** `core/secp256k1.cpp:23` - Incomplete inline assembly for multiplication
**Why it's wrong:** Only multiplies limb[0], missing limbs 1-3
**Do this instead:** Use the C++ fallback below (which is correct) or complete the assembly
### TODO Comment - Sequential Mode
**What happens:** `engine/address.cpp:515` - Sequential worker not parallelized yet
**Why it's wrong:** Threads created but workers not properly distributed
**Do this instead:** Follow hybrid mode pattern in `engine/address.cpp:638-651`
## Error Handling
**Strategy:** Return `bool` + `std::string& error` for all fallible operations
**Patterns:**
- `parse_*_cli()` returns `false` with error message on failure
- `load_*()` returns success indicator
- Checkpoint errors printed but don't halt execution
## Cross-Cutting Concerns
**Logging:** Direct `std::cout`/`std::cerr` - no logging framework
**Validation:** CLI validation returns errors before engine starts
**Authentication:** Not applicable (search engine)
---
*Architecture analysis: 2026-05-01*