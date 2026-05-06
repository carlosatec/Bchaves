---
last_mapped_date: 2026-05-02
---
# Architecture

## Design Overview
Bchaves is a high-performance, multi-threaded offline search engine optimized for solving discrete logarithm problems on the secp256k1 elliptic curve (Bitcoin private key space). It heavily relies on Data-Oriented Design (DOD) principles to maximize cache locality and CPU pipeline saturation.

## System Components & Data Flow

1. **CLI & Initialization (`system/cli.cpp`, `engine/app.cpp`)**
   - Parses arguments and determines the mode of execution.
   - Loads search targets from disk.
   - Initiates hardware detection to set optimal batch sizes (Auto-Tune profiles: `safe`, `balanced`, `max`).

2. **Search Engines (`engine/`)**
   - **Address Mode**: Primary bit-range exploration tool. Fuses endomorphism (GLV) to process related keys in parallel.
   - **Kangaroo Mode**: Pollard's Kangaroo algorithm implementation for large discrete log jumps. Includes trap detection.
   - **BSGS Mode**: Baby-Step Giant-Step memory-optimized algorithm for collision finding.

3. **Cryptographic Kernel (`core/`)**
   - Coordinates are mapped across SIMD lanes.
   - Elliptic curve point additions/doublings are batched.
   - Outputs are pushed through vectorized SHA-256 and RIPEMD-160 pipelines.

4. **Filtering & Matching**
   - Cuckoo Filters and Adaptive Filters (`core/adaptive_filter.cpp`) quickly eliminate non-matching hashes without memory bottlenecks, avoiding expensive hash table lookups for negative results.

5. **Persistence Layer (`system/checkpoint.cpp`)**
   - Atomic checkpoints are saved periodically, ensuring that interruptions (SIGINT) do not cause loss of search progress.