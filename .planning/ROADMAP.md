# Project Roadmap

## 🎯 Milestone 1: Core Engine Stabilization

### Phase 1: Fix Cryptographic Kernel (DONE)
- [x] Fix `reduce_p256_64` carry ripple bug.
- [x] Fix `mod_square_k1` alignment and carry bug.
- [x] Replace broken `mod_inv` binary GCD with robust Fermat path.
- [x] Correct Tonelli-Shanks constant for Secp256k1.
- [x] Fix `parse_big_int` hex detection heuristic.

### Phase 2: Validation & Quality Assurance (DONE)
- [x] Run `crypto_test` and verify all EC operations (100% pass).
- [x] Benchmark Kangaroo mode with Puzzle #135 target (Success).
- [x] Audit `mod_mul_k1` for overflow edge cases (Verified).

### Phase 3: Persistence & Checkpoint Audit (DONE)
- [x] Stress test checkpoint save/load in Kangaroo mode.
- [x] Verify Cuckoo Filter integrity after large-scale trap dumps.
- [x] Implement trap file versioning check (v3).
- [x] Fix Hybrid mode checkpoint restoration (restoring progress counter).
- [x] Implement signal handling and emergency save in BSGS engine.

### Phase 4: Final Polish & Benchmarking (DONE)
- [x] Comprehensive throughput benchmarking for all engines.
- [x] Documentation update for all CLI flags.
- [x] Final multi-arch validation.

### Phase 5: Kangaroo SSE2 Optimization (DONE)
- [x] Implement SSE2 SIMD hashing (SHA256/RIPEMD160).
- [x] Replace `std::unordered_map` with high-performance `FlatMap`.
- [x] Increase trap sharding to 64 to reduce mutex contention.
- [x] Tune prefetching and cache alignment for DDR3 systems.
- [x] Implement assembly-optimized arithmetic kernels.

### Phase 6: Hardware Detection & Auto Mode (DONE)
- [x] Enhance CPU feature detection (AVX-512 sub-features, CPU brand string)
- [x] Add L1/L2 cache detection (currently hardcoded)
- [x] Add NUMA topology detection
- [x] Improve physical core detection (AMD SMT-aware)
- [x] Add memory bandwidth estimation
- [x] Hardware-adaptive auto-derive profiles
- [x] Add `--list-hardware` CLI flag

**Plans:** 1 plan
- [x] 06-01-PLAN.md — Enhanced hardware detection and auto-derive with improved auto-tuning

### Phase 7: Fix Critical Crypto Implementation Issues (COMPLETED)
- [x] Fix ARM64 64-bit modular multiplication (uses 32-bit split)
- [x] Fix SSE4/AVX2/AVX512 modular multiplication (implemented with 512-bit accumulators)
- [x] Implement Jacobian point addition for all backends (SSE4, AVX2, AVX512, ARM64)
- [x] Fix invalid intrinsic macro usage in system/hardware.cpp (FIXED)

**Plans:** 1 plan
- [x] 07-01-PLAN.md — Fix secp256k1 SIMD implementation bugs (COMPLETED & VALIDATED)

---

### Phase 8: Fleet Batching Integration (COMPLETED)
- [x] Implement SIMD point addition dispatcher in `kangaroo.cpp` (COMPLETED)
- [x] Refactor `KangarooWorkerState` to support vectorized point sets (COMPLETED)
- [x] Implement batch normalization for parallel distinguished point detection (COMPLETED)
- [x] Optimize memory prefetching for vectorized jump tables (COMPLETED)
- [x] Final performance benchmarking vs target (~2.14 M/s on i3-7020U) (COMPLETED)

**Plans:** 1 plan
- [x] phases/08-fleet-integration/08-01-PLAN.md — Integrate SIMD kernels into Kangaroo search engine (COMPLETED)

**Review:** [08-REVIEW.md](phases/08-fleet-integration/08-REVIEW.md)

---

### Phase 9: Adaptive Multi-Address Cuckoo Filters (DONE)
- [x] Implement `AdaptiveCuckooFilter` with ISA-specific dispatch (AVX512/AVX2/SSE4/NEON)
- [x] Integrate "Zero-Cost" hashing for HASH160 fingerprints
- [x] Implement cache-line alignment and optimized SIMD kernels
- [x] Integrate filter into `Address` search loop with batch lookup
- [x] Validate performance via unit tests and WSL build

---

### Phase 10: ARM64 Crypto & Hash Acceleration (DONE)
- [x] Implement SHA256 with ARMv8 Crypto Extensions (sha256h/sha256h2).
- [x] Implement RIPEMD160 NEON vectorized kernels.
- [x] Optimize Secp256k1 NEON arithmetic for AArch64.
- [x] Integrate hardware-adaptive dispatch for ARM features in `AdaptiveCuckooFilter`.
- [x] **Global Project Audit**: Code deduplication, assembly safety check, and architectural review completed.

---

## 🏁 Milestone 3: Professional Grade Hardening (In Progress)

### Fase 11: Resiliência e Otimização Avançada (DONE)
- [x] **Checkpoint Integrity**: Implementar soma de verificação (CRC32) para validar a integridade dos arquivos `.ckp`.
- [x] **NUMA-Aware Allocation**: Otimizar alocação de memória para processadores multi-socket (afinidade de nó).
- [x] **Manual Prefetching**: Integrar `_mm_prefetch` nos loops críticos do Cuckoo Filter.
- [x] **Structure Refactoring**: Renomear `core/address.cpp` para `core/bitcoin_format.cpp` e centralizar telemetria.
- [x] **Unit Testing Expansion**: Adicionar testes de estresse para condições de corrida na troca de contexto de sinal.

### Fase 12: Memory-Wall Breakthrough & Zero-Copy I/O (DONE)
- [x] **HugePages Allocator**: Implementar alocador nativo de páginas enormes (2MB via `madvise(MADV_HUGEPAGE)` no Linux, `MEM_LARGE_PAGES` via `VirtualAlloc` no Windows) para CuckooFilter e TrapTables, eliminando penalidades de TLB miss.
- [x] **Distinguished Point SIMD Masking**: Substituir o loop escalar `is_distinguished` por máscara vetorial pura (`_mm256_cmpeq_epi64` / `_mm512_cmpeq_epi64_mask`), testando a frota inteira de 1024 cangurus num único passo sem branch escalar.
- [x] **Software Pipelining (Prefetch Distance 4-8)**: Evoluir o prefetching de distância 1 para pipeline de 3 estágios (prefetch em `i+8`, preparo em `i+4`, comparação em `i`) nos kernels do AdaptiveCuckooFilter, cobrindo a latência total de acesso à DRAM (~100ns).
- [x] **Zero-Copy Trap I/O (mmap)**: Substituir `fstream` por memory-mapped files (`mmap` / `CreateFileMapping`) para leitura e escrita de armadilhas no Kangaroo, delegando paginação e I/O ao kernel do SO.
- [x] **Flat Hash Map para Dump**: Substituir `std::unordered_map` em `dump_shards_to_disk` por hash map aberto inline (open-addressing flat map), eliminando alocações de heap por nó e melhorando cache locality (ganho estimado 2-4x no merge).
- [x] **False Sharing Guard**: Alinhar `g_stop_requested` e demais `std::atomic` globais com `alignas(64)` para isolar cada flag em sua própria cache line e eliminar ping-pong no barramento de coerência.

**Review:** [12-REVIEW.md](phases/12-memory-wall-breakthrough/12-REVIEW.md)

---
*Generated by GSD Lifecycle Management.*
