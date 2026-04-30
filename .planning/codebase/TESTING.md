# Testing Strategy

## Overview
The project uses a custom verification suite located in `tests/` to validate cryptographic correctness and engine performance.

## Test Types

### 1. Crypto Verification (`tests/crypto_test.cpp`)
- Validates field arithmetic (add, sub, mul, inv).
- Validates ECC point operations (add, double, multiply).
- Validates GLV decomposition correctness.
- Validates hash implementations (SHA256, RIPEMD160) against known vectors.

### 2. Engine Benchmarking
- Search modules include internal performance reporting (keys/second).
- Validation via "found" key detection for known targets.

## Running Tests
To run the standard verification suite:
```powershell
make test
./build/crypto_test
```

## Future Testing Goals
- Integration of a unit test framework (e.g., GTest or Catch2).
- Continuous integration (CI) for multi-arch builds (x86 and ARM).
- Stress testing for long-running search persistence (checkpoint/resume).
