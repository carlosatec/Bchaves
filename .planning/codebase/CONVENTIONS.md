# Conventions

Last mapped: 2026-05-06

## Code Style

- **Standard:** C++17
- **Namespace:** `bchaves::core`, `bchaves::engine`, `bchaves::system`
- **Naming:**
  - Functions/variables: `snake_case` (`mod_mul_k1`, `batch_normalize`)
  - Types/structs: `PascalCase` (`BigInt`, `Secp256k1Point`, `FleetState`)
  - Constants: `kPascalCase` (`kFieldPrime`, `kCurveOrder`, `kGLV_Lambda`)
  - Enums: `snake_case` values (`auto_select`, `address_btc`)
  - Macros: `UPPER_SNAKE_CASE` (rare — almost none used)
- **File naming:** `snake_case.cpp`, ISA suffix with dash (`sha256-avx2.cpp`)

## Header Patterns

- All headers use `#pragma once`
- No include guards (`#ifndef/#define`) — `#pragma once` exclusively
- Headers contain inline utilities when performance-critical (e.g., `bytes32_to_bigint` in `secp256k1.hpp`)
- ISA-specific headers (`secp256k1-avx2.hpp`) are included conditionally via `#ifdef` or runtime dispatch

## Error Handling

- **No exceptions** — the entire codebase is exception-free
- Error reporting via return codes (`bool` success/failure) and `std::optional`
- `fprintf(stderr, ...)` for user-facing error messages in engines
- `__builtin_expect` used for branch prediction hints on hot paths

## Memory Management

- Stack allocation preferred over heap for hot paths
- `alignas(32)` / `alignas(64)` for SIMD-aligned buffers
- Raw `new`/`aligned_alloc` for large filter allocations (`AdaptiveCuckooFilter::m_buckets`)
- No smart pointers in hot paths (performance-critical)
- `std::vector` for cold-path collections only

## Threading

- `std::thread` for worker threads (no thread pool abstraction)
- `std::atomic<bool>` for cross-thread signaling (`g_stop_requested`, `found`)
- `std::mutex` + `std::lock_guard` for shared state protection (trap insertion)
- Thread affinity via `pin_thread_to_core()` / `pin_thread_to_node()`
- No `std::async` or futures — all parallelism is manual

## SIMD Patterns

- Runtime ISA dispatch via function pointers (`CuckooKernel` struct)
- Each ISA has its own `.cpp` file — no `#ifdef` spaghetti within a single file
- Intrinsics used directly (`_mm256_*`, `_mm512_*`, `vld1q_*`) — no wrapper abstraction
- `__builtin_prefetch` / `_mm_prefetch` for manual memory prefetching

## Build Conventions

- Single `Makefile` at project root
- All sources listed explicitly (no glob patterns)
- `-O3 -flto` for all targets (no debug builds defined)
- Architecture detection via `uname -m` in Makefile
- No separate debug/release configurations

## Documentation

- File-level comments with project name, description, author, license
- Inline comments in Portuguese for domain logic, English for technical/API
- No Doxygen or generated docs — comments are the primary documentation