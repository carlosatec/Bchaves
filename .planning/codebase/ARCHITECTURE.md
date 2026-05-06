# Architecture

Last mapped: 2026-05-06

## Pattern

**Layered Monolith** — 4 layers with strict dependency direction (top → bottom):

```
┌─────────────────────────────────────────┐
│  modulos/   Entry points (main())       │  ← User-facing binaries
├─────────────────────────────────────────┤
│  engine/    Search algorithms           │  ← Business logic
├─────────────────────────────────────────┤
│  system/    Platform services           │  ← OS abstraction
├─────────────────────────────────────────┤
│  core/      Cryptographic primitives    │  ← Pure math, zero OS deps
└─────────────────────────────────────────┘
```

## Layers

### Layer 1: `modulos/` — Entry Points
- `modulos/address.cpp` — `main()` for Address search binary
- `modulos/bsgs.cpp` — `main()` for Baby-step Giant-step binary
- `modulos/kangaroo.cpp` — `main()` for Pollard's Kangaroo binary

Each calls `bchaves::engine::run_*()` after parsing CLI args.

### Layer 2: `engine/` — Search Engines
- `engine/address.cpp` (44 KB) — Hybrid multi-mode address search with endomorphism fusion, chunked processing, and AdaptiveCuckooFilter integration
- `engine/bsgs.cpp` (15 KB) — Baby-step Giant-step with GLV decomposition and hash table lookup
- `engine/kangaroo.cpp` (39 KB) — Pollard's Kangaroo with fleet-based SIMD point arithmetic, distinguished points, trap persistence, and signal handling
- `engine/app.cpp` / `engine/app.hpp` — Shared engine initialization (hardware detection, banner, target loading)

### Layer 3: `system/` — Platform Abstraction
- `system/hardware.cpp` (23 KB) — CPU feature detection (AVX/SSE/NEON), cache topology, NUMA detection, thread affinity (`pin_thread_to_core`, `pin_thread_to_node`)
- `system/cli.cpp` (14 KB) — CLI argument parsing for all 3 engine modes
- `system/checkpoint.cpp` (10 KB) — Binary checkpoint persistence with CRC32 integrity validation
- `system/targets.cpp` (9 KB) — Multi-format Bitcoin target file parsing (P2PKH, P2SH, Bech32, HASH160, pubkeys)
- `system/format.cpp` (5 KB) — Telemetry formatting (throughput, ETA, memory)
- `system/io.cpp` (2 KB) — File I/O utilities
- `system/types.hpp` (5 KB) — Core type definitions: `HardwareInfo`, `CommonOptions`, engine-specific option structs, `CheckpointState`

### Layer 4: `core/` — Cryptographic Primitives
- `core/secp256k1.cpp` (39 KB) — Full Secp256k1 elliptic curve arithmetic: field math, point operations, GLV decomposition, Jacobian coordinates, batch normalization
- `core/secp256k1_fleet.hpp` (7 KB) — SoA (Structure of Arrays) fleet layout for vectorized Kangaroo operations
- `core/secp256k1_reduce.hpp` (2 KB) — Modular reduction constants
- `core/secp256k1-{sse4,avx2,avx512,arm64}.hpp` — ISA-specific SIMD point arithmetic kernels
- `core/hash.cpp` (21 KB) — SHA-256, RIPEMD-160, HASH160 with multi-ISA dispatch
- `core/sha256-{sse4,avx,avx2,avx512,arm64}.cpp` — ISA-specific SHA-256 implementations
- `core/ripemd160-{sse4,avx2,neon}.cpp` — ISA-specific RIPEMD-160 implementations
- `core/adaptive_filter.cpp` (6 KB) — AdaptiveCuckooFilter with runtime ISA dispatch
- `core/adaptive_filter_{avx2,avx512,sse4,neon}.cpp` — SIMD-optimized batch lookup kernels with manual prefetching
- `core/bitcoin_format.cpp` (4 KB) — Bitcoin address derivation (WIF, Base58Check, pubkey compression)
- `core/base58.cpp` (3 KB) — Base58 encoding/decoding

## Data Flow

```
CLI args → parse_cli() → CommonOptions
         → detect_hardware() → HardwareInfo
         → load_targets() → TargetEntry[]
         → AdaptiveCuckooFilter(targets)
         → run_{address|bsgs|kangaroo}()
              ├── Worker threads (pinned to cores)
              │    ├── secp256k1_multiply / secp256k1_add (point ops)
              │    ├── hash160() → SHA256 → RIPEMD160
              │    ├── filter.lookup_batch() → SIMD comparison
              │    └── if match → verify against target list
              ├── Checkpoint timer (periodic save)
              └── Signal handler (SIGINT → emergency save)
```

## Key Abstractions

| Abstraction | Type | Purpose |
|---|---|---|
| `BigInt` | struct (4×u64 limbs) | 256-bit integer for Secp256k1 scalars |
| `Secp256k1Point` | struct (x, y, infinity) | Affine curve point |
| `PointJacobian` | struct (x, y, z) | Projective point for batch operations |
| `FleetState` | SoA struct | Vectorized fleet of 1024 Kangaroo states |
| `AdaptiveCuckooFilter` | class | Hardware-adaptive probabilistic set membership |
| `CuckooKernel` | dispatch struct | Function pointers for ISA-specific lookup |
| `HardwareInfo` | struct | CPU features, cache, NUMA topology |
| `CheckpointState` | struct | Serializable engine state for persistence |