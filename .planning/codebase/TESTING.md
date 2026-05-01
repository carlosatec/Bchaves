# Testing Patterns

**Analysis Date:** 2026-05-01

## Test Framework

**Status:** No testing infrastructure detected

**Observation:** No test files found in any directory

**Config Files:** None found
- No `CMakeLists.txt` with test targets
- No `Makefile` with test rules
- No `catch2`, `gtest`, or other test framework includes

## Test File Organization

**Location:** Not applicable - no tests exist

**Recommendation:** Establish tests in:
```
tests/
├── unit/
│   ├── test_bigint.cpp
│   ├── test_hash.cpp
│   └── test_secp256k1.cpp
├── integration/
│   ├── test_address_derivation.cpp
│   └── test_checkpoint.cpp
└── bench/
    ├── bench_sha256.cpp
    └── bench_kangaroo.cpp
```

## Test Structure Patterns

**Not applicable** - no tests to analyze

**Recommended patterns for this codebase:**

### Unit Test Structure
```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("BigInt addition", "[bigint]") {
    bchaves::core::BigInt a(1);
    bchaves::core::BigInt b(2);
    bchaves::core::BigInt c = a + b;
    REQUIRE(c == bchaves::core::BigInt(3));
}
```

### Property-Based Tests (for crypto)
```cpp
TEST_CASE("BigInt associativity", "[bigint][property]") {
    for (int i = 0; i < 1000; ++i) {
        BigInt a = random_bigint();
        BigInt b = random_bigint();
        BigInt c = random_bigint();
        REQUIRE((a + b) + c == a + (b + c));
    }
}
```

## Mocking

**Framework:** Not applicable - no mocking observed

**What to Mock (recommended):**
- Hardware detection: mock `detect_hardware()` for consistent CI
- File I/O: mock checkpoint/trap file operations
- Threading: mock thread creation for deterministic tests

**What NOT to Mock:**
- Cryptographic primitives: test actual implementations
- BigInt arithmetic: verify with known test vectors
- secp256k1 operations: use known elliptic curve points

## Fixtures and Factories

**Not applicable** - no tests exist

**Recommended patterns:**

### Test Fixtures
```cpp
struct CryptoFixture {
    BigInt test_private_key;
    Secp256k1Point expected_point;
    
    CryptoFixture() {
        // Known test vector from literature
        test_private_key = BigInt(1);
    }
};
```

### Factory Functions
```cpp
BigInt random_bigint(std::uint32_t bits) {
    BigInt result;
    for (int i = 0; i < 4; ++i) {
        result.limbs[i] = rand() | (rand() << 16);
    }
    return result;
}
```

## Test Types Needed

**Unit Tests:**
- BigInt operations (add, subtract, multiply, divide)
- Hash functions (SHA-256, RIPEMD160)
- secp256k1 point operations
- Base58 encoding/decoding
- Address derivation
- CLI argument parsing

**Integration Tests:**
- Full address derivation from private key
- Checkpoint save/load cycle
- Trap file persistence and loading

**Property Tests:**
- BigInt mathematical properties (associativity, distributivity)
- Point addition identity: P + O = P
- Curve order property: G * n = O (point at infinity)

**Performance Tests:**
- Hash throughput (hashes/second)
- Key derivation throughput
- Kangaroo/BSGS search rate

## Coverage

**Current:** 0% - no tests exist

**Recommended Targets:**
- Core crypto: 90%+
- CLI parsing: 80%+
- Checkpoint I/O: 90%+

**View Coverage (recommended):**
```bash
cmake -DCMAKE_CXX_FLAGS="--coverage" ...
lcov --capture --directory . --output-file coverage.info
lcov --summary coverage.info
```

## Known Test Vectors

**For BigInt:**
```cpp
// From secp256k1 specification
BigInt one(1);
BigInt curve_order = secp256k1_curve_order();  // n
REQUIRE(secp256k1_multiply(one).x != 0);  // Generator point
```

**For Address Derivation:**
```cpp
// Bitcoin wiki test vectors
BigInt private_key(0);
std::string expected_wif = "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn";
// ... verify address matches
```

**For SHA-256:**
```cpp
// Known test vector
std::string input = "abc";
auto hash = sha256(input);
std::array<uint8_t, 32> expected = {/* known SHA-256("abc") */};
REQUIRE(hash == expected);
```

## Async Testing

**Not applicable** - no async code tested

**Recommendation:** Use `std::async` with timeout for thread tests:
```cpp
TEST_CASE("Worker completes within timeout", "[thread]") {
    auto future = std::async(std::launch::async, run_search, options);
    auto status = future.wait_for(std::chrono::minutes(5));
    REQUIRE(status == std::future_status::ready);
}
```

## Error Testing

**Recommendation:**
```cpp
TEST_CASE("Invalid private key rejected", "[address]") {
    BigInt zero;
    DerivedKeyInfo info;
    REQUIRE(!derive_key_info(zero, info));  // Must fail
    
    BigInt too_big = curve_order + 1;
    REQUIRE(!derive_key_info(too_big, info));  // Must fail
}
```

---

*Testing analysis: 2026-05-01*