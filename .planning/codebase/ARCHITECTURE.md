<!-- refreshed: 2026-05-01 -->
# Architecture

**Analysis Date:** 2026-05-01

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    modulos/ (CLI Entrypoints)               │
│  `[modulos/address.cpp]`, `[modulos/bsgs.cpp]`, `[modulos/kangaroo.cpp]`
├─────────────────────────────────────────────────────────────┤
│                     engine/ (Search Engines)                │
│  `[engine/app.hpp]`, `[engine/address.cpp]`, `[engine/bsgs.cpp]`, `[engine/kangaroo.cpp]`
├─────────────────────────────────────────────────────────────┤
│                      core/ (Crypto Primitives)               │
│  `[core/secp256k1.hpp]`, `[core/hash.hpp]`, `[core/address.hpp]`, `[core/cuckoo.hpp]`
├─────────────────────────────────────────────────────────────┤
│                    system/ (Infrastructure)                │
│  `[system/cli.hpp]`, `[system/hardware.hpp]`, `[system/checkpoint.hpp]`, `[system/targets.hpp]`
└─────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| modulos/ | CLI entry points that parse arguments and invoke engines | `[modulos/address.cpp]`, `[modulos/bsgs.cpp]`, `[modulos/kangaroo.cpp]` |
| engine/ | Search algorithms orchestration, worker threads, batch processing | `[engine/address.cpp]`, `[engine/bsgs.cpp]`, `[engine/kangaroo.cpp]`, `[engine/app.cpp]` |
| core/ | Cryptographic operations: secp256k1, hashing, BigInt, address derivation | `[core/secp256k1.hpp]`, `[core/hash.hpp]`, `[core/address.hpp]`, `[core/cuckoo.hpp]` |
| system/ | Infrastructure: CLI parsing, hardware detection, I/O, checkpoints | `[system/cli.hpp]`, `[system/hardware.hpp]`, `[system/checkpoint.hpp]`, `[system/targets.hpp]` |

## Pattern Overview

**Overall:** Layered Pipeline Architecture with Worker Thread Pool

**Key Characteristics:**
- Clear separation between CLI parsing (modulos), algorithm orchestration (engine), cryptographic operations (core), and system services (system)
- Multi-threaded search with thread pinning to CPU cores
- Checkpoint/resume capability for long-running searches
- Cuckoo Filter for fast target lookups (O(1) probabilistic)
- Batch processing with Jacobian point arithmetic for performance

## Layers

**modulos/ (CLI Entry Points):**
- Purpose: Command-line interface entry points that parse user arguments and invoke engine functions
- Location: `[modulos/]`
- Contains: `main()` functions for each search algorithm
- Depends on: `engine/` (run_address, run_bsgs, run_kangaroo), `system/` (CLI parsing)
- Used by: End user command line

**engine/ (Search Algorithm Orchestration):**
- Purpose: Implements search algorithms, manages worker threads, handles batch processing
- Location: `[engine/]`
- Contains: Address search (linear + hybrid chunk), BSGS algorithm, Kangaroo (Fleet model)
- Depends on: `core/` (cryptography), `system/` (hardware detection, checkpoints, targets, formatting)
- Used by: `modulos/` entry points

**core/ (Cryptographic Primitives):**
- Purpose: Low-level cryptographic operations - secp256k1 elliptic curve, SHA-256, RIPEMD160, BigInt arithmetic
- Location: `[core/]`
- Contains: Secp256k1 point multiplication (portable + GLV endomorphism), SHA-256 (AVX2 batched), RIPEMD160, Base58 encoding, Cuckoo Filter
- Depends on: None (lowest layer - self-contained)
- Used by: `engine/` search algorithms

**system/ (Infrastructure Services):**
- Purpose: Platform abstraction - hardware detection, thread pinning, CLI parsing, file I/O, checkpoint persistence
- Location: `[system/]`
- Contains: Hardware detection, auto-tuning profiles, target file loading, checkpoint save/load, formatting
- Depends on: None (uses standard C++ library)
- Used by: `engine/` and `modulos/`

## Data Flow

### Primary Request Path (Address Search)

1. **Entry Point** - `modulos/address.cpp:main()` - parses CLI arguments via `[system/cli.cpp]`
2. **Options Creation** - `[system/types.hpp:AddressOptions]` struct created with parsed parameters
3. **Engine Invocation** - `[engine/app.cpp:run_address()]` called with options
4. **Hardware Detection** - `[system/hardware.cpp:detect_hardware()]` queries CPU features and memory
5. **Auto-Tuning** - `[system/hardware.cpp:tune_for()]` calculates thread count and batch size
6. **Target Loading** - `[system/targets.cpp:load_targets()]` parses target file, creates Cuckoo Filter
7. **Range Calculation** - `[engine/address.cpp:resolve_range()]` computes start/end based on `-b bits`
8. **Worker Spawn** - `[engine/address.cpp]` spawns N threads (one per CPU core)
9. **Thread Loop** - Each worker iterates through range, computes public key → hash160 → compares against targets
10. **Match Found** - `[engine/app.cpp:report_found()]` outputs result and optionally saves to `found.txt`

### BSGS Search Path

1. **Entry Point** - `modulos/bsgs.cpp:main()` - parses `-b bits` and `-k table_k`
2. **Baby Steps Phase** - `[engine/bsgs.cpp]` pre-computes table of n*m points, stores in shards with Cuckoo Filter
3. **Giant Steps Phase** - Worker threads iterate `target - i*step*G`, lookup in Cuckoo Filter + binary search in shards
4. **Match** - When filter matches, full binary search verifies exact match, computes private key = baby + giant

### Kangaroo Search Path

1. **Entry Point** - `modulos/kangaroo.cpp:main()` - parses `-r start:end` range
2. **Cold Boot** - `[engine/kangaroo.cpp:load_traps_from_disk()]` loads previous traps from disk (if any)
3. **Fleet Initialization** - Each thread manages 64 kangaroos (wild/tame ratio configurable)
4. **Jump Table** - 64-entry pre-computed jump table with exponential distances
5. **Search Loop** - Each kangaroo jumps randomly based on x-coordinate, inserts trap when distinguished point found
6. **Collision Detection** - When wild meets tame, private key is computed: `candidate = range_end + distance_wild - distance_tame`

**State Management:**
- Global atomic counters for progress tracking
- Mutex-protected shared state for found key reporting
- Per-worker state for sequential/hybrid checkpointing

## Key Abstractions

**BigInt (256-bit):**
- Purpose: Represent secp256k1 private keys and curve points
- Examples: `[core/secp256k1.hpp:BigInt]`
- Pattern: 4x uint64_t limbs array

**Secp256k1Point:**
- Purpose: Elliptic curve point (x, y)
- Examples: `[core/secp256k1.hpp:Secp256k1Point]`
- Pattern: Jacobian coordinates for batch processing, affine for output

**PointJacobian:**
- Purpose: Internal representation for fast point arithmetic
- Examples: `[core/secp256k1.hpp:PointJacobian]`
- Pattern: Uses z-coordinate for efficient addition/doubling

**CuckooFilter:**
- Purpose: Probabilistic set membership for fast target lookup
- Examples: `[core/cuckoo.hpp:CuckooFilter]`
- Pattern: 16-bit fingerprints, 4 slots per bucket, cuckoo hashing

**AddressMatcher:**
- Purpose: Holds target addresses for search comparison
- Examples: `[engine/app.hpp:AddressMatcher]`
- Pattern: Sorted vector + Cuckoo Filter for O(log n) binary search + O(1) filter pre-check

## Entry Points

**modulos/address.cpp:**
- Location: `[modulos/address.cpp]`
- Triggers: `bchaves --address -b 32 -t targets.txt`
- Responsibilities: Parse CLI, invoke run_address(), handle return codes

**modulos/bsgs.cpp:**
- Location: `[modulos/bsgs.cpp]`
- Triggers: `bchaves --bsgs -b 40 -k 1024 -t pubkey.hex`
- Responsibilities: Parse CLI, invoke run_bsgs(), handle return codes

**modulos/kangaroo.cpp:**
- Location: `[modulos/kangaroo.cpp]`
- Triggers: `bchaves --kangaroo -r 1:FFFFFFFFFFFFFFFF -t pubkey.hex`
- Responsibilities: Parse CLI, invoke run_kangaroo(), handle trap persistence

## Architectural Constraints

- **Threading:** Worker threads spawned per CPU core, pinned via `[system/hardware.cpp:pin_thread_to_core()]`
- **Global state:** Atomic counters for progress (`g_chunk_counter` in hybrid mode), signal handler for graceful shutdown
- **Circular imports:** None - clear unidirectional dependency: modulos → engine → (core + system)
- **Memory:** Cuckoo Filter uses ~10% of available RAM, Kangaroo traps use ~80% of available RAM

## Anti-Patterns

### Direct BigInt Arithmetic in Hot Loop

**What happens:** Some inner loops perform individual BigInt operations instead of batch operations
**Why it's wrong:** Each BigInt operation involves 4-limb arithmetic; batch operations amortize overhead
**Do this instead:** Use `[core/secp256k1.hpp:batch_normalize()]` and `[core/secp256k1.hpp:add_points_mixed()]` for batch processing

### Missing AVX2 Detection Fallback

**What happens:** Code assumes AVX2/SHA-NI hardware support without runtime capability check
**Why it's wrong:** Crashes on older CPUs without these extensions
**Do this instead:** Use `[core/hash.hpp:Sha256::supports_avx2()]` and `[core/hash.hpp:Sha256::supports_shani()]` before enabling SIMD paths

## Error Handling

**Strategy:** Return codes + stderr messages + optional checkpoint save on interrupt

**Patterns:**
- Return 0 on success, non-zero on error
- Print `[E]` prefix for errors, `[+]` for status, `[*]` for progress
- On SIGINT, save checkpoint before exit (if enabled)

## Cross-Cutting Concerns

**Logging:** Console output only via `std::cout`/`std::cerr` with progress indicators
**Validation:** Input validation in CLI parser, curve order validation in secp256k1
**Authentication:** Not applicable (this is a search engine, not an authentication system)
**Thread Safety:** Atomics for counters, mutexes for shared state (found key, worker state snapshot)

---

*Architecture analysis: 2026-05-01*