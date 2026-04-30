# Architecture

## Overview
Bchaves is a high-performance search engine for Bitcoin keys, designed with a modular architecture that separates cryptographic primitives from search algorithms and system-level infrastructure.

## Component Layers

### 1. Arithmetic & Crypto Kernel (`core/`)
- **secp256k1**: The heart of the engine. Implements point addition, doubling, multiplication, and GLV decomposition.
- **Hash Functions**: Optimized implementations of SHA256 and RIPEMD160.
- **Data Structures**: Cuckoo Filters for high-throughput public key matching.

### 2. Search Engines (`engine/`)
- **Address Engine**: Direct search for addresses (P2PKH, P2SH, etc.).
- **BSGS Engine**: Baby-step Giant-step algorithm for discrete logarithm problems.
- **Kangaroo Engine**: Pollard's Kangaroo algorithm for range-based searches.
- **App Wrapper**: Orchestrates the setup and teardown of search jobs.

### 3. System Infrastructure (`system/`)
- **Hardware Abstraction**: Detects CPU features (AVX, ARM Crypto) to select the best arithmetic implementation.
- **Persistence**: Handles checkpoints to allow resuming interrupted searches.
- **CLI & I/O**: Command-line interface and file logging.

## Design Patterns
- **Modular Dispatch**: Entry points in `modulos/` link against common engine and core libraries.
- **Resource Management**: Uses RAII and smart pointers for memory management.
- **Zero-Copy Paths**: Designed to minimize data movement in the hot loop of search kernels.
