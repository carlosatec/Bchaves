# Testing

Last mapped: 2026-05-06

## Framework

**Custom test framework** — no external testing library (no gtest, Catch2, etc.). Tests use a simple macro-based assertion system with `assert()` and manual pass/fail reporting.

## Test Files

| File | Tests | Scope |
|---|---|---|
| `tests/crypto_test.cpp` | 73 | EC operations, hashing, encoding, modular arithmetic, GLV decomposition, BigInt ops, checkpoint roundtrip, CLI contracts |
| `tests/simd_test.cpp` | ~4 | SIMD kernel structural validation (mul64x64, point_add per ISA) |
| `tests/test_cuckoo_adaptive.cpp` | ~4 | AdaptiveCuckooFilter insert, lookup, batch lookup |
| `tests/stress_test.cpp` | 1 | Multi-threaded signal handling stress (10 SIGINT under concurrency) |
| `tests/test_crypto_arm64.cpp` | ~5 | ARM64-specific crypto validation (conditional on aarch64) |

## Test Execution

```bash
# Run all tests via Makefile
make test

# Execution order:
# 1. build/crypto_test     → 73 assertions
# 2. build/simd_test       → SIMD structural checks
# 3. build/cuckoo_test     → Cuckoo Filter ops
# 4. build/stress_test     → Signal resilience
```

## Test Categories

### Unit Tests (`crypto_test.cpp`)
- `test_wif` — WIF encoding/decoding
- `test_address` — P2PKH address derivation
- `test_bigint_ops` — BigInt arithmetic (add, sub, mul, shift)
- `test_glv_decomposition` — GLV scalar decomposition correctness
- `test_mul_small` — Small scalar multiplication
- `test_bytes32_roundtrip` — BigInt ↔ byte array serialization
- `test_mod_arithmetic` — Modular add/sub/mul/square
- `test_batch_normalize` — Jacobian → Affine batch conversion
- `test_cli_contracts` — CLI flag parsing validation
- `test_checkpoint_roundtrip` — Checkpoint save/load with CRC32

### SIMD Validation (`simd_test.cpp`)
- Tests each ISA backend in isolation
- Structural checks (function executes without crash, basic correctness)
- Not exhaustive — relies on `crypto_test` for mathematical correctness

### Integration Tests (`test_cuckoo_adaptive.cpp`)
- Insert/lookup correctness
- Batch lookup (exercises SIMD dispatch path)
- No false-positive rate benchmarking (yet)

### Stress Tests (`stress_test.cpp`)
- Fires 10 concurrent `SIGINT` signals
- Verifies no deadlocks in signal handler
- Validates atomic flag consistency under thread contention

## Coverage

- **Covered:** EC arithmetic, hashing, encoding, checkpoint persistence, CLI parsing, signal handling
- **Not covered:** Full engine integration tests (no end-to-end search validation in CI), Kangaroo trap file I/O, BSGS hash table edge cases

## Running Tests

Tests are built and run via WSL:
```bash
wsl make test
```

All tests must pass with exit code 0. Output format:
```
=== Results: 73 passed, 0 failed ===
[+] ALL TESTS PASSED
```