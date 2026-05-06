---
last_mapped_date: 2026-05-02
---
# Directory Structure

- `core/`
  Contains all the cryptographic primitives, mathematical algorithms, and SIMD filters. This is the "hot path" of the application. Includes:
  - `secp256k1*`: Elliptic curve arithmetic kernels (scalar, AVX2, AVX512, SSE4, ARM64).
  - `sha256*`, `ripemd160*`: SIMD hashing implementations.
  - `adaptive_filter*`, `cuckoo.hpp`: High-speed memory filtering mechanisms.

- `engine/`
  Contains the high-level orchestration logic for the different search strategies:
  - `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`, `app.cpp`.

- `system/`
  Contains all Operating System and hardware interaction wrappers:
  - `cli.cpp`: Command line argument parsing.
  - `hardware.cpp`: CPU architecture and capability detection.
  - `checkpoint.cpp`: Progress saving and loading logic.
  - `targets.cpp`, `io.cpp`: File reading and writing.

- `tests/`
  Unit test suites for verifying cryptographic correctness and SIMD implementation equivalency:
  - `crypto_test.cpp`, `simd_test.cpp`, `test_cuckoo_adaptive.cpp`.

- `modulos/`
  Entry-point templates (e.g. `modulos/address.cpp`) that wrap the engine execution for the generated binaries.

- `doc/`
  Markdown documentation files detailing the project's architecture, setup, and usage.

- `traps/`
  (Likely empty or data dir) Directory for storing Pollard's Kangaroo trap points.

- `scratch/`
  Temporary development files.

- `build/`
  Output directory for compiled object files and final binaries.