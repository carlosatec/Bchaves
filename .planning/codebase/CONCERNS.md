---
last_mapped_date: 2026-05-02
---
# Concerns

## Technical Debt & Maintenance
- **SIMD Duplication**: Implementing and maintaining 5 different architectural paths (Scalar, SSE4, AVX2, AVX512, ARM NEON) for each algorithm (SHA-256, RIPEMD-160, SECP256K1) creates a huge maintenance burden. Bug fixes in one ISA implementation must be manually ported and verified across all others.
- **Hardcoded Optimizations**: Hand-tuned algorithmic constants (e.g., `mod_mul_k1`) are extremely brittle and require advanced domain knowledge to modify safely.

## Bugs & Fragile Areas
- **Checkpoint Resilience**: While atomic, manual filesystem checkpoints (`system/checkpoint.cpp`) are vulnerable to I/O bottlenecks and potential corruption if not carefully synchronized across massive thread pools (especially under load in Kangaroo/BSGS modes).
- **Auto-Tune Complexity**: The hardware heuristic engine could misidentify newer CPUs or exotic NUMA topologies, resulting in suboptimal thread allocation or cache trashing.

## Security
- **Memory Analysis**: Due to the offline nature of the tool, remote attacks are unlikely, but the heavy use of raw pointers and manual memory alignment in the SIMD filters requires extreme caution to avoid out-of-bounds reads/writes.