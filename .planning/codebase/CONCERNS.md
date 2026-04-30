# Technical Concerns & Risks

## Arithmetic Robustness
- **Modular Reduction Overflows**: High-range Bitcoin keys (e.g., Puzzle #135) require precise handling of modular arithmetic. Previous issues with `mod_square_k1` and `mod_mul_k1` suggest that any changes to the arithmetic kernel must be heavily validated.
- **Deserialization Logic**: Critical failure identified in Kangaroo engine when processing high-range public keys. Point coordinate reconstruction (quadratic residue handling) is a known weak point.

## Performance & Hardware
- **Legacy Hardware Support**: Xeon servers lacking AVX2 instructions are severely limited in throughput. Optimizations like Cuckoo Filters and prefetching are essential but complex to implement without regression on modern CPUs.
- **ARM64 Optimization**: Maintaining parity between x86 (AVX2/AVX-512) and ARM64 (Crypto Extensions) without forking the codebase significantly.

## Search Scalability
- **Collision Detection**: As target lists grow (millions of addresses), the memory overhead and lookup latency of the collision filter become bottlenecks.
- **Persistence Integrity**: Checkpoint corruption or version mismatch can lead to massive loss of search progress.

## Code Quality
- **Lack of Unit Framework**: Heavy reliance on custom test scripts (`crypto_test.cpp`) rather than a standard framework like GTest/Catch2 makes it harder to add granular tests for new features.
- **Modular Dispatch**: The separation between `modulos/` and `engine/` is clean but requires careful management of link-time dependencies to avoid binary bloat.
