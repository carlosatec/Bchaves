# Project Structure

## Directory Map

| Directory | Purpose |
| :--- | :--- |
| `core/` | Cryptographic primitives (Secp256k1, Hash, Base58, RIPEMD160). |
| `engine/` | Search algorithm implementations (Address, BSGS, Kangaroo). |
| `modulos/` | CLI entry points for different search modes. |
| `system/` | System utilities (CLI, Hardware, Checkpoint, I/O, Types). |
| `tests/` | Unit and integration tests for crypto and search logic. |
| `doc/` | Project documentation and technical specs. |
| `build/` | Output directory for compiled binaries. |
| `puzzles/` | Placeholder/Configuration for specific puzzle targets. |
| `traps/` | (Internal) Trap handling or specialized search markers. |

## Key Files
- `Makefile`: Central build configuration.
- `engine/app.hpp`: Main entry point for engine orchestration.
- `core/secp256k1.hpp`: Core arithmetic definitions.
- `system/types.hpp`: Global type definitions and constants.
- `README.md`: Project overview and usage instructions.
