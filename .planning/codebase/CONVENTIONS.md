# Coding Conventions

**Analysis Date:** 2026-05-01

## Naming Patterns

**Files:**
- C++ source: lowercase with underscores: `hash.cpp`, `secp256k1.cpp`
- C++ headers: lowercase with underscores: `hash.hpp`, `secp256k1.hpp`
- Pattern: `[module]/[component].{cpp|hpp}`

**Functions:**
- PascalCase for most functions: `derive_key_info`, `load_targets`
- Some internal functions use snake_case: `bytes32_to_bigint`
- Pattern: Mixed, but majority is PascalCase

**Variables:**
- camelCase for local variables: `range_start`, `num_threads`
- Member variables use `m_` prefix: `m_capacity`, `m_mask`
- Global variables use `g_` prefix: `g_interrupt_requested`
- Constants use `k` prefix: `kFleetSize`, `kBabyBatch`

**Types:**
- PascalCase for classes/structs: `BigInt`, `TrapTable`, `KangarooWorkerState`
- Enums use PascalCase with descriptive names: `SearchMode`, `AutoTuneProfile`

## Code Style

**Formatting:**
- No explicit formatting config found (no `.clang-format`)
- 4-space indentation observed
- Braces on same line for control structures

**Linting:**
- No `.clang-tidy` or `.eslintrc` found
- No CI lint checks observed

**Bracing Style:**
```cpp
if (condition) {
    // code
} else {
    // code
}
```

## Import Organization

**Order:**
1. System includes: `<iostream>`, `<vector>`, etc.
2. Project headers: `"core/..."`, `"engine/..."`, `"system/..."`
3. No clear separation between groups

**Path Aliases:**
- None observed - using full relative paths

**Example from `engine/bsgs.cpp`:**
```cpp
#include "engine/app.hpp"
#include "core/secp256k1.hpp"
#include "core/cuckoo.hpp"
#include "core/hash.hpp"
#include "system/checkpoint.hpp"

#include <algorithm>
#include <ctime>
#include <vector>
```

## Error Handling

**Patterns:**
- Return `bool` for functions that can fail: `parse_address_cli()`, `load_checkpoint()`
- Use `std::string& error` parameter for error messages
- Throw `std::runtime_error` in CLI parser for invalid inputs
- No RAII-based error propagation

**Example (system/cli.cpp:101-140):**
```cpp
bool parse_address_cli(int argc, char** argv, AddressOptions& options, std::string& error) {
    try {
        // parsing logic
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    return ensure_target(options, error);
}
```

**Inconsistent Patterns:**
- Some functions return error codes (`int` 0/1)
- Some use exceptions
- Some silently fail

## Logging

**Framework:** Console output via `std::cout` and `std::cerr`

**Patterns:**
- Progress output: `std::cout << "[+] ..."`
- Errors: `std::cerr << "[E] ..."`
- Warnings: `std::cout << "[!] ..."`
- Debug: None observed

**Example (engine/bsgs.cpp:101-102):**
```cpp
std::cout << "[+] Iniciando BSGS (Cuckoo Filter Accelerated)\n";
std::cerr << "[E] Falha ao configurar secp256k1: " << backend_error << '\n';
```

## Comments

**When to Comment:**
- File headers with license and purpose: `/* Bchaves: Bitcoin Performance Engine ... */`
- Complex algorithm sections with phase descriptions
- TODO comments for incomplete features

**JSDoc/TSDoc:** Not applicable (C++ project)

**Example (engine/kangaroo.cpp:39-42):**
```cpp
// ============================================================
// Estruturas de Dados
// ============================================================
```

## Function Design

**Size:** Various - some small helpers, some 200+ line functions

**Parameters:**
- Use references for output parameters: `bool load_targets(const Path&, TargetLoadResult& out)`
- Use const references for input: `const BigInt& private_key`
- Use raw pointers for interop: `uint8_t* out`

**Return Values:**
- Return by value for small types: `std::array<std::uint8_t, 32>`
- Return by const reference for large objects: `const BigInt&`
- Return bool for success/failure functions

## Module Design

**Exports:**
- Namespace-scoped functions in `bchaves::core`, `bchaves::engine`, `bchaves::system`
- No explicit export decorators (single-header library approach)

**Barrel Files:** Not used

## Architectural Patterns

**Overall:** Component-based with clear separation:
- `core/` - Cryptographic primitives
- `engine/` - Search algorithms
- `system/` - Infrastructure (CLI, hardware detection, I/O)
- `modulos/` - Entry points

**Key Patterns:**
- Builder pattern for options parsing
- Worker thread pattern for parallel search
- Checkpoint pattern for state persistence
- Trap/Cache pattern for kangaroo algorithm

## Code Specific Conventions

**Memory Management:**
- Use smart pointers: `std::unique_ptr`, `std::make_unique`
- Avoid raw `new`/`delete`
- Aligned allocations use `_aligned_malloc`/`aligned_alloc` with matching `free`

**Threading:**
- `std::thread` for worker threads
- `std::mutex` for shared data
- `std::atomic` for counters/flags
- `std::memory_order_relaxed` for performance

**Crypto Operations:**
- BigInt as `std::array<std::uint64_t, 4>`
- Jacobian coordinates for point arithmetic
- Batch normalization for performance

---

*Convention analysis: 2026-05-01*