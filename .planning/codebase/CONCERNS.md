# Concerns

Last mapped: 2026-05-06

## Performance Bottlenecks

### P-01: TLB Pressure on Large Allocations (High Impact)
- **Files:** `core/adaptive_filter.cpp`, `engine/kangaroo.cpp`
- **Issue:** The Cuckoo Filter and Kangaroo trap tables allocate tens of GB using standard 4KB pages. Each page requires a TLB entry, causing 5-15% invisible performance tax from TLB misses.
- **Mitigation:** Implement HugePages allocation (2MB/1GB) via `madvise(MADV_HUGEPAGE)` (Linux) and `MEM_LARGE_PAGES` (Windows). Planned for Phase 12.

### P-02: Scalar Distinguished Point Check (Medium Impact)
- **File:** `engine/kangaroo.cpp:691-697`
- **Issue:** The `is_distinguished` check iterates through 1024 fleet elements sequentially using scalar operations, breaking the SIMD flow of the Kangaroo engine.
- **Mitigation:** Replace with vectorized mask (`_mm512_cmpeq_epi64_mask`). Planned for Phase 12.

### P-03: std::unordered_map in Hot Path (Medium Impact)
- **File:** `engine/kangaroo.cpp:383`
- **Issue:** `dump_shards_to_disk` uses `std::unordered_map` for trap merging, causing heap fragmentation and poor cache locality due to node-based allocation.
- **Mitigation:** Replace with open-addressing flat hash map. Planned for Phase 12.

### P-04: Prefetch Distance Too Short (Low-Medium Impact)
- **Files:** `core/adaptive_filter_avx2.cpp`, `*_avx512.cpp`, `*_sse4.cpp`, `*_neon.cpp`
- **Issue:** Manual prefetching targets `i+1` which only hides ~20-30% of DRAM latency (~100ns). A distance of 4-8 elements is needed for full latency hiding.
- **Mitigation:** Software pipelining with 3-stage prefetch. Planned for Phase 12.

### P-05: False Sharing on Atomic Flags (Low Impact)
- **File:** `engine/kangaroo.cpp:38`
- **Issue:** `std::atomic<bool> g_stop_requested` lacks `alignas(64)`, potentially sharing a cache line with adjacent data and causing unnecessary coherence traffic across cores.
- **Mitigation:** Add `alignas(64)` to all global atomics. Planned for Phase 12.

## Technical Debt

### D-01: Legacy `core/address.cpp` / `core/address.hpp` Still in Repository
- These files were renamed to `bitcoin_format` but the old names may still exist as tracked files in some branches.
- **Action:** Verify clean removal via `git ls-files`.

### D-02: No Debug Build Configuration
- The Makefile only supports `-O3 -flto`. There is no debug build with `-g -O0 -fsanitize=address`.
- **Risk:** Makes debugging difficult; no AddressSanitizer or UndefinedBehaviorSanitizer in CI.

### D-03: `modulos/` Directory Purpose Unclear
- Contains thin `main()` wrappers that just call engine functions. Could be merged into engine or renamed to `bin/`.

### D-04: No End-to-End Integration Tests
- Tests validate individual components but no test runs a full search against known puzzle solutions.

## Security

### S-01: Signal Handler Complexity
- **File:** `engine/kangaroo.cpp:39`
- Signal handler sets an atomic bool, which is safe. However, the checkpoint save triggered on SIGINT happens in the main thread after signal, not in the handler itself — this is correct practice.

### S-02: No Input Sanitization on Target Files
- **File:** `system/targets.cpp`
- Target file parsing trusts input format. Malformed hex strings could potentially cause issues, though the parser does validate target types.

## Architecture Risks

### A-01: Monolithic Engine Files
- `engine/kangaroo.cpp` (39 KB) and `engine/address.cpp` (44 KB) are large single-file implementations. Refactoring into smaller modules would improve maintainability.

### A-02: No Abstraction Over ISA Dispatch
- Each algorithm (SHA-256, RIPEMD-160, Secp256k1, CuckooFilter) implements its own ISA dispatch mechanism. A unified dispatch pattern could reduce duplication.