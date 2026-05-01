# Coding Conventions
**Analysis Date:** 2026-05-01

## Naming Patterns
**Files:**
- `.cpp` extension for implementation files
- `.hpp` extension for header files
- Pattern: `component_name.cpp`, `component_name.hpp`
- Examples: `core/hash.cpp`, `engine/address.cpp`, `system/cli.hpp`

**Directories:**
- Lowercase category names: `core/`, `engine/`, `system/`, `modulos/`, `tests/`
- Pattern: `<category>/<component>.<ext>`

**Classes/Structs:**
- PascalCase with descriptive names: `Sha256`, `BigInt`, `DerivedKeyInfo`, `AddressMatcher`, `SequentialWorkerState`
- Suffix patterns: `Info` for data carriers, `Options` for configuration, `State` for runtime state
- Examples: `Secp256k1Point`, `PointJacobian`, `CheckpointState`, `TuneProfile`

**Functions:**
- snake_case: `sha256`, `derive_key_info`, `bigint_div_u64_checked`, `batch_mod_inv_k1`
- Verb-noun pattern: `load_targets`, `resolve_range`, `serialize_pubkey`, `run_hybrid_worker`
- Prefix patterns: `parse_*`, `load_*`, `save_*`, `run_*`, `check_*`

**Variables:**
- snake_case: `start`, `end`, `chunk_key_count`, `worker_currents`
- Global/static: `g_` prefix: `g_use_shani`, `g_chunk_counter`, `g_chi_`
- Constants: `k` prefix: `kFieldPrime`, `kCurveOrder`, `kGLV_Lambda`, `kBatch`
- Private members: `_` suffix: `data_length_`, `state_`, `buffer_`, `infinity_`
- Loop indices: single letter or `i`, `j`, `k`, `u` for inner loops
- Arrays: descriptive plural or numbered: `batch_offsets`, `Gn[512]`, `pub_bufs[8][65]`

**Enums:**
- PascalCase enum name: `TargetType`, `SearchMode`, `SearchType`, `AutoTuneProfile`
- PascalCase values: `TargetType::address_btc`, `SearchMode::sequential`, `SearchType::both`
- Integer backing types: `std::uint32_t` for flags, `std::size_t` for counts

**Type Aliases:**
- PascalCase: `ByteVector` for `std::vector<std::uint8_t>`

## Code Style
**Formatting:**
- 4-space indentation (tabs converted to spaces)
- Braces on same line: `if (condition) {`
- Body indented one level: `do_something();`
- Blank line between logical blocks
- Max line length ~120 chars (wraps at discretion)

**Casting:**
- Always use `static_cast<T>()` for explicit type conversions: `static_cast<std::uint32_t>(length)`
- Never use C-style casts in new code
- Use `reinterpret_cast` only in low-level SIMD code with clear comments

**Type Usage:**
- Fixed-width integers: `std::uint8_t`, `std::uint32_t`, `std::uint64_t`, `std::size_t`
- Use `<cstdint>` header for portable types
- Avoid `unsigned` shorthand in favor of `std::uint*_t`
- SIMD types: `__m128i`, `__m256i` (intrinsics), `alignas(16)` for alignment

**Memory:**
- Use `std::vector` for dynamic arrays
- Use `std::array` for fixed-size arrays: `std::array<std::uint8_t, 32>`
- Prefer `std::unique_ptr` for heap-allocated objects: `std::make_unique<CuckooFilter>()`
- Raw `new`/`delete` prohibited in new code

## Import Organization
**Order in source files:**
1. Engine layer includes: `#include "engine/app.hpp"`
2. Core includes: `#include "core/address.hpp"`, `#include "core/hash.hpp"`
3. System includes: `#include "system/checkpoint.hpp"`, `#include "system/format.hpp"`
4. Standard library: `#include <algorithm>`, `#include <vector>`, `#include <atomic>`
5. System headers: `#include <cstring>`, `#include <mutex>`

**Path Aliases:**
- Not configured; use relative paths from repo root
- Pattern: `#include "core/hash.hpp"` (quotes for internal)
- Pattern: `#include <vector>` (angle brackets for standard)

**Header Dependencies:**
- Use `#pragma once` guard in all headers
- Forward declare where possible to reduce coupling
- Include only what's needed in each header

## Preprocessor & Build
**Compiler Flags:**
- C++17 standard: `-std=c++17`
- Optimization: `-O3 -flto` (link-time optimization)
- Architecture: `-march=native` (auto-detected)
- SSE2 fallback: `-march=westmere -msse2 -mno-avx`
- Warnings: `-Wall -Wextra`

**Platform Guards:**
```cpp
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin. h>
#endif
```

**SIMD Target Hints:**
```cpp
#if defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
__attribute__((target("sha,sse4.1")))
#endif
#endif
```

## Error Handling
**Return Value Pattern:**
- Bool returns for functions that can fail: `bool derive_key_info(...)`, `bool load_targets(...)`
- Success: return `true`
- Failure: return `false` or `!`
- Use output parameters for error details

**Error Reporting:**
```cpp
std::string error;
bool ok = some_function(args, error);
if (!ok) {
    std::cerr << "[E] " << error << '\n';
    return 1;
}
```

**Validation:**
- Check inputs at function entry
- Return early on invalid input
- Provide descriptive error messages

**Exception Policy:**
- No exceptions used
- All error handling via return codes and output parameters
- Checkpoint saves are silent on success, report on failure

## Threading & Concurrency
**Atomic Operations:**
- Use `std::atomic<T>` for shared counters: `std::atomic<uint64_t> total_processed{0}`
- Memory ordering: `std::memory_order_relaxed` for performance in tight loops
- `fetch_add`, `load`, `store` operations

**Mutex Usage:**
```cpp
std::mutex found_mutex;
std::lock_guard<std::mutex> lock(found_mutex);
// protected operation
```

**Thread Management:**
- Create threads with `std::thread` and `emplace_back`
- Use RAII guards for cleanup: `WorkerExitGuard` struct pattern
- Thread pinning: `bchaves::system::pin_thread_to_core(i)`

**Once Initialization:**
```cpp
static std::once_flag init_gn;
std::call_once(init_gn, [&](){
    // one-time initialization
});
```

## Comments
**File Headers:**
```cpp
/*
 * Bchaves: Bitcoin Performance Engine
 *
 * Descrição: [Purpose in Portuguese]
 *
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
```

**Inline Comments:**
- `// Comment for non-obvious code` (inline, explaining why)
- `// TODO: Description` for incomplete features
- `// FIXME: Description` for known issues
- Avoid block comments for inline documentation

**API Documentation:**
```cpp
/**
 * @brief Reporta uma chave encontrada: exibe na tela e salva em found. txt.
 */
void report_found(...);
```

## Function Design
**Size:**
- Functions tend to be large due to performance requirements
- Complex lambdas for batch processing: `check_batch_fn`, `hash_and_check`
- Extract helper functions for repeated patterns

**Parameters:**
- Const references for large objects: `const BigInt&`
- Output parameters via reference: `DerivedKeyInfo& out`
- Pointers for arrays: `const std::uint8_t* data[8]`

**Return Values:**
- Single return values preferred
- Bool for success/failure with output parameter
- Complex types via output parameter

## Module Design
**Public Interface:**
- Exposed via header files
- Namespaced: `bchaves::core`, `bchaves::engine`, `bchaves::system`
- Factory functions for complex initialization

**Internal Helpers:**
- Anonymous namespace blocks:
```cpp
namespace {
    bool g_uses_shani = false;
    void init_dispatch() { ... }
}  // namespace
```

**Exports:**
- No explicit `__declspec(dllexport)` needed (static library)
- All public functions declared in headers

## SIMD Intrinsics
**Naming Macros:**
```cpp
#define ROTR4(x, n) _mm_or_si128(_mm_srli__epi32(x, n), _mm_slli_epi32(x, 32 - n))
#define SIG0_4(x) XOR4(ROTR4(x, 7), XOR4(ROTR4(x, 18), SHR4(x, 3)))
```

**Pattern:**
- Uppercase macro names for SIMD operations
- Parenthesized arguments in all macros
- Fallback implementations when intrinsics unavailable

---
*Convention analysis: 2026-05-01*