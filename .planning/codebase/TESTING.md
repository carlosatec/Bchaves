---
last_mapped_date: 2026-05-02
---
# Testing

## Framework & Execution
- The project implements its own bespoke testing binaries rather than relying on a heavy framework like GoogleTest.
- Tests are executed via `make test`, which builds and runs the test executables sequentially.

## Test Binaries
1. **`crypto_test` (`tests/crypto_test.cpp`)**:
   - Validates the base mathematical correctness of the custom `secp256k1` arithmetic.
   - Ensures scalar multiplication matches expected public keys.
2. **`simd_test` (`tests/simd_test.cpp`)**:
   - Ensures that the various parallelized SIMD hashing routines (SSE4, AVX2, AVX512, NEON) produce identical outputs to the baseline scalar reference.
3. **`cuckoo_test` (`tests/test_cuckoo_adaptive.cpp`)**:
   - Validates the high-speed collision filtering logic and memory management.

## Coverage
Coverage primarily focuses on data integrity (cryptographic correctness) and hardware-specific path equivalency. System integration tests (e.g., verifying a full kangaroo search end-to-end) appear to be managed manually via CLI testing.