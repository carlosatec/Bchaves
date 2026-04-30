# Project: Bchaves (Bitcoin Performance Engine)

## 🎯 Vision
Bchaves is an ultra-high performance Bitcoin private key search engine, optimized for low-level hardware performance and advanced elliptic curve mathematics. It aims to provide the fastest possible search throughput for public keys in the Secp256k1 curve.

## 🚀 Core Features
- **Hybrid-Endo Fusion Engine**: Native GLV endomorphism integration.
- **Optimized Field Arithmetic**: Custom kernels for Secp256k1 prime.
- **Native AVX2 SIMD**: Parallel SHA-256 and RIPEMD-160 hashing.
- **Checkpoint Protection**: Robust progress persistence.
- **Large-Scale Cuckoo Filter**: O(1) probabilistic target matching.
- **Multi-Arch**: Supports x86_64 and ARM64 (Apple Silicon/Graviton).

## 🛠️ Technology Stack
- **Language**: C++17
- **SIMD**: AVX2 (Intel/AMD), NEON (ARM64 via compatibility)
- **Build System**: Makefile
- **Math**: Custom Secp256k1 Jacobian and Affine coordinate arithmetic with GLV optimization.

## 📂 System Modules
- **`address`**: Bit-range explorer.
- **`kangaroo`**: Pollard's Kangaroo algorithm.
- **`bsgs`**: Baby-Step Giant-Step engine.

## 📍 Project Status
- **Current Goal**: Final benchmarking and CLI documentation polish after persistence engine stabilization.
- **Next Milestone**: Milestone 2: Multi-Arch Production Release.
