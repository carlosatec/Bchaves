# Codebase Structure
**Analysis Date:** 2026-05-01
## Directory Layout
```
[project-root]/
├── .planning/         # GSD planning artifacts
├── build/            # Compiled binaries (generated)
├── core/             # Cryptographic core
├── doc/              # Project documentation
├── engine/           # Search engine implementations
├── modulos/         # CLI entry points
├── puzzles/         # Puzzle definitions
├── system/           # System infrastructure
├── tests/           # Test sources
├── traps/            # Trap file storage
├── CONTRIBUTING.md  # Contribution guide
├── LICENSE          # MIT license
├── Makefile         # Build configuration
├── QUALITY_SCAN.md  # Code quality report
├── README.md        # Project overview
└── tutorial-gsd.md # Tutorial
```
## Directory Purposes
**`core/`:**
- Purpose: Low-level cryptographic primitives
- Contains: `secp256k1.cpp`, `address.cpp`, `hash.cpp`, `base58.cpp`, `hash_table.cpp`, `cuckoo.cpp`
- Key files: `secp256k1.cpp` (1128 lines), `hash.cpp`

**`engine/`:**
- Purpose: Search algorithm implementations
- Contains: `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`, `app.cpp`
- Key files: `address.cpp` (883 lines) - main search logic

**`system/`:**
- Purpose: System services (CLI, I/O, checkpointing)
- Contains: `cli.cpp`, `io.cpp`, `checkpoint.cpp`, `hardware.cpp`, `format.cpp`, `targets.cpp`

**`modulos/`:**
- Purpose: Executable entry points
- Contains: `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`
- Note: These are thin wrappers that call engine functions

**`tests/`:**
- Purpose: Test and validation programs
- Contains: `crypto_test.cpp`

**`doc/`:**
- Purpose: Project documentation
- Contains: `ARCHITECTURE.md`, `CONFIGURATION.md`, `DEVELOPMENT.md`, `GETTING-STARTED.md`, `TESTING.md`

**`build/`:**
- Purpose: Generated binary output directory
- Generated: Yes
- Committed: No (in .gitignore)
## Key File Locations
**Entry Points:**
- `modulos/address.cpp`: Address search CLI entry
- `modulos/bsgs.cpp`: BSGS search CLI entry
- `modulos/kangaroo.cpp`: Kangaroo search CLI entry
- `tests/crypto_test.cpp`: Cryptographic test runner

**Configuration:**
- `Makefile`: Build targets (lines 9-72)
- `system/types.hpp`: Type definitions and enums

**Core Logic:**
- `core/secp256k1.cpp`: Secp256k1 elliptic curve implementation
- `engine/address.cpp`: Address search implementation
- `core/address.cpp`: Bitcoin address derivation

**Core Headers:**
- `core/secp256k1.hpp`: Secp256k1 interface
- `core/address.hpp`: Address derivation interface
- `core/hash.hpp`: Hash function interface
- `core/cuckoo.hpp`: Cuckoo filter interface

**System Headers:**
- `system/cli.hpp`: CLI parsing interface
- `system/checkpoint.hpp`: Checkpoint interface
- `system/hardware.hpp`: Hardware detection interface
- `system/types.hpp`: Common types and enums
- `system/targets.hpp`: Target loading interface
- `engine/app.hpp`: Engine orchestration interface

**System Implementation:**
- `system/cli.cpp`: CLI argument parsing
- `system/checkpoint.cpp`: Checkpoint save/load
- `system/hardware.cpp`: CPU detection and affinity
- `system/io.cpp`: Result file I/O
- `system/format.cpp`: Output formatting
- `system/targets.cpp`: Target file parsing
## Naming Conventions
**Files:**
- Pattern: `*.cpp` for implementations, `*.hpp` for headers
- Example: `engine/address.cpp`, `core/secp256k1.hpp`

**Functions:**
- Pattern: `snake_case` or `lower_snake_case`
- Example: `parse_address_cli()`, `run_address()`, `load_targets()`

**Types/Structs:**
- Pattern: `PascalCase`
- Example: `Secp256k1Point`, `AddressMatcher`, `CheckpointState`

**Namespaces:**
- Pattern: `bchaves::core`, `bchaves::engine`, `bchaves::system`
- Example: `bchaves::core::Secp256k1Point`

**Constants:**
- Pattern: `kPrefix` (Hungarian notation)
- Example: `kFieldPrime`, `kCurveOrder`, `kGLV_Beta`
## Where to Add New Code
**New Search Algorithm:**
- Primary code: `engine/` directory
- Entry point: New file in `modulos/` + Makefile target
- Example: Copy `modulos/address.cpp` → `modulos/newmode.cpp`

**New Cryptographic Primitive:**
- Implementation: `core/` directory
- Header: `core/newprimitive.hpp`
- Example: Copy `core/secp256k1.cpp` pattern

**New System Service:**
- Implementation: `system/` directory
- Header: `system/newservice.hpp`
- Example: Copy `system/checkpoint.cpp` pattern

**Utilities:**
- Shared helpers: Appropriate `system/` or `core/` file
- Don't create new utility files for small helpers

**Tests:**
- Test program: `tests/` directory
- Example: `tests/crypto_test.cpp`
## Special Directories
**`build/`:**
- Purpose: Compiled executables and intermediates
- Generated: Yes (by `make all`)
- Committed: No

**`traps/`:**
- Purpose: Persistent trap storage for kangaroo mode
- Generated: Yes (by kangaroo engine)
- Committed: No

**`puzzles/`:**
- Purpose: Bitcoin puzzle definitions
- Generated: No
- Committed: Yes
## Source File Summary
| File | Lines | Purpose |
|------|-------|---------|
| `core/secp256k1.cpp` | 1128 | Secp256k1 elliptic curve math |
| `engine/address.cpp` | 883 | Address search implementation |
| `system/cli.cpp` | 267 | CLI argument parsing |
| `core/secp256k1.hpp` | 119 | Secp256k1 interface |
| `system/types.hpp` | 164 | Common types |
| `core/hash.cpp` | ~400 | SHA-256, RIPEMD-160 |
| `engine/app.cpp` | 41 | Engine orchestration |
| `system/checkpoint.cpp` | ~200 | Checkpoint persistence |
---
*Structure analysis: 2026-05-01*