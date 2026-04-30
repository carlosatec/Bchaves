# Tech Stack

## Programming Languages
- **C++17**: Core language for the engine, chosen for performance and low-level control.

## Build System
- **GNU Make**: Multi-target build system supporting Windows and Linux.

## Core Libraries & Primitives
- **Secp256k1**: Custom optimized arithmetic kernel for elliptic curve operations.
- **Hash Functions**: Implementation of SHA256, RIPEMD160.
- **Encoding**: Base58Check for Bitcoin addresses.
- **Data Structures**: Cuckoo Filter for high-speed collision detection.

## Hardware Support
- **x86_64**: Native optimizations (march=native).
- **ARM64**: Specialized support for ARMv8.2-a with crypto extensions.

## Tools
- **G++ / Clang**: C++ compilers.
- **Pthreads**: (Implicitly used by most parallel engines, though need to verify in code).
