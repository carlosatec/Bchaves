<!-- refreshed: 2026-05-01 -->
# Codebase Structure

**Analysis Date:** 2026-05-01

## Directory Layout

```
[project-root]/
├── modulos/              # CLI entry points
│   ├── address.cpp       # Address search CLI
│   ├── bsgs.cpp          # BSGS search CLI
│   └── kangaroo.cpp      # Kangaroo search CLI
├── engine/               # Search algorithm implementations
│   ├── app.hpp           # Engine orchestration (run_address, report_found)
│   ├── app.cpp           # Engine utilities
│   ├── address.cpp       # Address search (linear + hybrid)
│   ├── bsgs.cpp          # Baby-step giant-step algorithm
│   └── kangaroo.cpp      # Pollard's kangaroo (fleet model)
├── core/                 # Cryptographic primitives
│   ├── secp256k1.hpp     # Secp256k1 curve (BigInt, point arithmetic)
│   ├── secp256k1.cpp     # Secp256k1 implementation
│   ├── hash.hpp          # SHA-256, RIPEMD160 (batched + portable)
│   ├── hash.cpp          # Hash implementation
│   ├── address.hpp       # Address derivation (P2PKH, P2SH, bech32)
│   ├── address.cpp       # Address derivation implementation
│   ├── base58.hpp        # Base58 encoding/decoding
│   ├── base58.cpp        # Base58 implementation
│   ├── cuckoo.hpp        # Cuckoo filter for fast lookups
│   ├── ripemd160.hpp     # RIPEMD-160 hash
│   └── hash_table.hpp    # Trap table for kangaroo
├── system/               # Infrastructure services
│   ├── cli.hpp           # CLI argument parsing
│   ├── cli.cpp           # CLI implementation
│   ├── types.hpp         # Common types (options, enums, structs)
│   ├── hardware.hpp      # Hardware detection & auto-tuning
│   ├── hardware.cpp      # Hardware implementation
│   ├── targets.hpp       # Target file loading
│   ├── targets.cpp       # Target loading implementation
│   ├── checkpoint.hpp   # Checkpoint save/load
│   ├── checkpoint.cpp   # Checkpoint implementation
│   ├── format.hpp        # Output formatting
│   ├── format.cpp        # Formatting implementation
│   ├── io.hpp            # File I/O (found.txt)
│   └── io.cpp            # I/O implementation
├── puzzles/              # Target address files (puzzles to solve)
│   ├── 1.txt             # Puzzle 1 target addresses
│   └── ...
└── doc/                  # Documentation
    ├── ARCHITECTURE.md   # Architecture overview
    ├── TESTING.md        # Testing patterns
    └── ...
```

## Directory Purposes

**modulos/:**
- Purpose: Command-line interface entry points for each search algorithm
- Contains: Single `main()` function files that parse CLI and invoke engine
- Key files: `[modulos/address.cpp]`, `[modulos/bsgs.cpp]`, `[modulos/kangaroo.cpp]`

**engine/:**
- Purpose: Search algorithm implementations - the core computational logic
- Contains: Address search (linear/hybrid), BSGS, Kangaroo with worker thread management
- Key files: `[engine/address.cpp]`, `[engine/bsgs.cpp]`, `[engine/kangaroo.cpp]`, `[engine/app.hpp]`

**core/:**
- Purpose: Low-level cryptographic primitives - no external dependencies
- Contains: Secp256k1 curve arithmetic, SHA-256, RIPEMD-160, Base58, Cuckoo Filter
- Key files: `[core/secp256k1.hpp]`, `[core/hash.hpp]`, `[core/cuckoo.hpp]`, `[core/address.hpp]`

**system/:**
- Purpose: Platform abstraction and infrastructure services
- Contains: CLI parsing, hardware detection, thread pinning, checkpoint persistence, target loading
- Key files: `[system/cli.hpp]`, `[system/hardware.hpp]`, `[system/checkpoint.hpp]`, `[system/targets.hpp]`

**puzzles/:**
- Purpose: Input files containing target addresses/public keys to search for
- Contains: Text files with one target per line (address, hash160, or pubkey)
- Generated: No - user-provided or pre-downloaded

## Key File Locations

**Entry Points:**
- `[modulos/address.cpp]`: Address search - `./bchaves --address -b 32 -t targets.txt`
- `[modulos/bsgs.cpp]`: BSGS search - `./bchaves --bsgs -b 40 -k 1024 -t pubkey.hex`
- `[modulos/kangaroo.cpp]`: Kangaroo search - `./bchaves --kangaroo -r start:end -t pubkey.hex`

**Configuration:**
- `[system/types.hpp]`: All option structures (`AddressOptions`, `BsgsOptions`, `KangarooOptions`)
- `[system/cli.cpp]`: CLI argument parsing and help text

**Core Logic:**
- `[core/secp256k1.hpp]`: BigInt (256-bit), Secp256k1Point, Jacobian coordinates, GLV endomorphism
- `[core/hash.hpp]`: Sha256 class with `hash4()`, `hash8()` for AVX2 batched hashing
- `[core/cuckoo.hpp]`: CuckooFilter for O(1) probabilistic target lookup

**Testing:**
- No dedicated test directory - tests appear to be manual or benchmark-driven via `--benchmark` flag

## Naming Conventions

**Files:**
- Pattern: `*.cpp` for implementations, `*.hpp` for headers
- Example: `address.cpp`, `address.hpp`

**Directories:**
- Pattern: Lowercase singular nouns
- Example: `core/`, `engine/`, `modulos/`, `system/`

**Namespaces:**
- Pattern: `bchaves::{layer}` (e.g., `bchaves::core`, `bchaves::engine`, `bchaves::system`)
- Example: `namespace bchaves::core { ... }`

**Types:**
- Pattern: PascalCase for classes/structs
- Example: `BigInt`, `Secp256k1Point`, `AddressOptions`

**Functions:**
- Pattern: snake_case
- Example: `run_address()`, `load_targets()`, `detect_hardware()`

**Enums:**
- Pattern: PascalCase enum name, PascalCase values
- Example: `SearchMode::sequential`, `TargetType::address_btc`

## Where to Add New Code

**New Search Algorithm (new engine):**
- Implementation: `[engine/myalgorithm.cpp]`
- Entry point: `[modulos/myalgorithm.cpp]` (new main)
- Declaration: Add to `[engine/app.hpp]`

**New Cryptographic Primitive:**
- Implementation: `[core/mycrypto.cpp]`
- Header: `[core/mycrypto.hpp]`
- Note: Keep core/ dependency-free

**New System Service:**
- Implementation: `[system/myservice.cpp]`
- Header: `[system/myservice.hpp]`
- Update: Add types to `[system/types.hpp]` if needed

**New Target Type:**
- Update: `[system/targets.cpp:detect_type()]`
- Add: Parse logic in `[system/targets.cpp]`

**New Hardware Feature Detection:**
- Update: `[system/hardware.cpp:detect_hardware()]`
- Add: CPU feature flag to `[system/types.hpp:CPUFeature]`

## Special Directories

**puzzles/:**
- Purpose: User-provided target files to search against
- Generated: No
- Committed: Yes (sample puzzles 1-99)

**doc/:**
- Purpose: Project documentation (architecture, testing, development guides)
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-05-01*