# Testing Patterns
**Analysis Date:** 2026-05-01
## Test Framework
**Runner:**
- No external test framework (Catch2, GTest, etc.)
- Custom regression harness in `tests/crypto_test. cpp`
- Compiled via Makefile: `make test` → builds and runs `tests/crypto_test. cpp`
- Exit code: 0 on all pass, 1 on any failure
**Assertion Library:**
- Standard `<cassert>` with custom `EXPECT` macro
- Comparison-based: `EXPECT(condition, "message")` prints `[FAIL] message` on false
- Counters track pass/fail: `g_pass`, `g_fail`
**Build Integration:**
- Makefile target:
```makefile
test: $(BUILD_DIR)/crypto_ test
    $(BUILD_DIR)/crypto_ test
$(BUILD_DIR)/crypto_test: tests/crypto_test. cpp $(COMMON_SOURCES) | $(BUILD_DIR)
    $(CXX) $(CXXFLAGS) $(COMMON_FLAGS) tests/crypto_test. cpp $(COMMON_SOURCES) -o $@
```
- Run via: `make test` (builds if needed, then runs)
- All Common_ SOURCES included for full cryptographic access
## Test File Organization
**Location:**
- Single file: `tests/crypto_test. cpp` (489 lines)
- All test functions in one file for simplicity
**Naming:**
- File: `crypto_test. cpp` (regression suite for cryptographic correctness)
- No separate test directory or co-located tests
- All tests use `test_` prefix: `test_sha256()`, `test_scalar_multiply()`
## Test Structure
**Suite Organization:**
```cpp
static int g_pass = 0;
static int g_fail = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << msg << "\n"; \
        ++g_fail; \
    } else { \
        ++g_pass; \
    } \
} while (0)
```
**Patterns:**
- Static counters and EXPECT macro at file scope
- Each test in a `static void test_*()` function
- Named sections in comments: `// ============================================================ // SHA-256: vetores de teste NIST // ============================================================`
- Console output: `std::cout << "[*] test_sha256\n";` to show progress
- Main calls each test in sequence
- Results summary at end: `std::cout << "\n=== Results: " << g_pass << " passed, " << g_fail << " failed ===\n";`
## Mocking
**Framework:** None
**Patterns:** No mocking; all tests use real cryptographic implementations against known test vectors
**What to Mock:** N/A - integration tests use full implementations
**What NOT to Mock:** Everything - cryptographic tests must use real implementations
## Fixtures and Factories
**Test Data:**
- Hard-coded test vectors from NIST/standards documents:
```cpp
// NIST SHA-256 test vector: SHA-256("")
const char* expected = "e3b0c442298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b78d52b855";
EXPECT(to_hex( std::vector<uint8_ t>(h.begin(), h.end())) == expected, "SHA-256 empty string");
```
- Bitcoin canonical test vectors for private keys, WIF, addresses:
```cpp
// Privkey = 1 → WIF compressed: KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn
BigInt priv(1);
DerivedKeyInfo info;
bool ok = derive_key_info(priv, info);
EXPECT(info.wif_compressed == "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn", "WIF compressed for privkey 1");
```
**Location:** All hard-coded in `crypto_test. cpp`
## Test Categories
**Unit Tests:** None explicitly separated; all tests are unit-level but use full implementations
**Regression Tests:**
- `test_sha256()` - NIST vectors
- `test_ripemd160()` - standard vectors
- `test_hash160_pubkey()` - Hash160 of compressed pubkey
- `test_scalar_multiply()` - secp256k1 multiplication G, 2G, 7G
- `test_pubkey_serde()` - serialization/deserialization round-trip
- `test_wif()` - WIF encoding from known private keys
- `test_address()` - P2PKH address generation
- `test_bigint_ops()` - BigInt arithmetic, carry propagation, comparison
- `test_glv_decomposition()` - GLV vs direct multiply consistency
- `test_mul_small()` - `mul_small_in_place` vs `operator*`
- `test_bytes32_roundtrip()` - big-endian byte serialization
- `test_mod_arithmetic()` - modular add/subtract
- `test_batch_normalize()` - Jacobian normalization
**Integration Tests:**
- `test_cli_contracts()` - CLI parsing validation (argument rejection, option storage)
- `test_checkpoint_roundtrip()` - checkpoint save/load file round-trip
## Common Patterns
**Cryptographic Vectors:**
```cpp
// SHA-256 known-answer test
const char* expected = "e3b0c44...";
auto h = sha256(data, length);
EXPECT(to_hex(std::vector<uint8_ t>(h.begin(), h.end())) == expected, "description");
```
**Round-Trip Testing:**
```cpp
// Serialize → deserialize → compare
uint8_ t buf[33];
serialize_pubkey(original, true, buf);
Secp256k1Point recovered = deserialize_pubkey(buf, 33);
EXPECT(!recovered.infinity, "deserialized not infinity");
EXPECT(recovered.x == original.x, "round-trip x matches");
EXPECT(recovered.y == original.y, "round-trip y matches");
```
**Comparison Testing:**
```cpp
// GLV must match direct computation
Secp256k1Point p_direct = secp256k1_ multiply(priv);
Secp256k1Point p_glv = secp256k1_multiply_glv(priv);
EXPECT(p_direct.x == p_glv.x, "GLV x matches direct x");
EXPECT(p_direct.y == p_glv.y, "GLV y matches direct y");
```
**CLI Contract Testing:**
```cpp
// Build argv array from string vector
std::vector< std::string> args = {"kangaroo", "target.txt", "-b", "75"};
std::vector< char*> argv = make_ argv(args);
EXPECT(!bchaves::system::parse_kangaroo_cli(static_ cast< int>(argv.size()), argv.data(), options, error), "rejects bit range 0");
EXPECT(error.find("1 e 256") != std::string:: npos, "reports bit range validation");
```
**File I/O Testing:**
```cpp
// Save → load → verify state
std::filesystem:: path temp = std::filesystem:: temp_directory_ path() / "bchaves_checkpoint_test.ckp";
EXPECT(bchaves::system::save_checkpoint(temp, state, error), "save checkpoint round-trip file");
EXPECT(bchaves::system::load_checkpoint(temp, loaded, error), "load checkpoint round-trip file");
EXPECT(loaded.algorithm == state.algorithm, "checkpoint algorithm round-trip");
// Cleanup
std::error_ code ec;
std::filesystem:: remove(temp, ec);
```
## Run Commands
```bash
make test              # Build and run crypto_test
make clean            # Clean build artifacts
make address          # Build address engine (no test run)
```
## Test Coverage
**Requirements:** None enforced (no coverage tool integration)
**Gaps:** No coverage tool (gcov/lcov); no automated coverage enforcement
- Manual verification via known-answer test vectors
- No test for error paths (only happy-path regression tests)
- No performance regression tests
- No stress tests or boundary condition tests
- No test for `SHA- NI` backend (disabled due to incomplete implementation)
## Test Philosophy
**Principle:** Cryptographic correctness via known-answer test vectors
- Every algorithm has at least one KAT (Known-Answer Test)
- Tests use published test vectors (NIST, Bitcoin canonical)
- Tests are deterministic, repeatable
- No random data, no timing-dependent tests
**Limitations:**
- Single regression file; no organization into categories
- Manual progress output (`[*] test_sha256`)
- No test framework features (parametrized tests, fixtures, etc.)
- No test isolation (each test runs in same process, state may leak)
---