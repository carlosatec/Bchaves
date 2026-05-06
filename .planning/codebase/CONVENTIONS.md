---
last_mapped_date: 2026-05-02
---
# Conventions

## Code Style
- **C++17 Features**: Extensive use of modern C++ where performance allows, but core algorithms heavily lean into C-style pointer arithmetic and explicit memory alignment for SIMD.
- **Header/Implementation Split**: Standard separation of declarations in `.hpp` and definitions in `.cpp`. However, many performance-critical math routines (especially in `secp256k1*`) are fully implemented inline in headers to guarantee inlining.
- **Naming**: 
  - Standard variables and functions generally use `snake_case`.
  - Architecture-specific files are suffixed (e.g. `sha256-avx2.cpp`, `secp256k1-arm64.hpp`).

## Patterns
- **Intrinsics Dispatching**: Algorithms check hardware capabilities at runtime (via `system/hardware.cpp`) or rely on compile-time flags (`#ifdef __AVX512F__`) to route to the appropriate SIMD implementation.
- **Manual Memory Management**: The core search loop intentionally avoids `std::vector` or dynamic allocations during hot execution paths. Arrays are statically allocated or aligned manually.
- **Type Aliasing**: Extensive use of fixed-width integer types (`uint32_t`, `uint64_t`) via `<cstdint>` to guarantee structural integrity across x86 and ARM platforms.

## Error Handling
- Exceptional states and parsing errors are handled synchronously and often log via custom formatters in `system/format.cpp`. Failures in checkpointing or critical logic typically result in clean program termination rather than exception bubbling to avoid overhead.