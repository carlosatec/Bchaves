# Tech Stack

## Core Language
- **C++17**: Utilizado para performance crítica e recursos modernos de metaprogramação (templates, constexpr).
- **Assembly/Intrinsics**: Uso extensivo de intrinsics SIMD (SSE4, AVX2, AVX512, NEON) para acelerar aritmética de curvas elípticas e hashing.

## Build & Toolchain
- **GNU Make**: Sistema de build principal via `Makefile`.
- **g++ (MinGW-w64 / GCC)**: Compilador padrão com flags de otimização agressiva (`-O3`, `-flto`).
- **SIMD Dispatch**: Seleção em tempo de compilação via macros de pré-processador baseadas na arquitetura alvo.

## Hardware Support
- **Multi-Arch**: Suporte nativo para x86_64 (Intel/AMD) e ARM64 (Apple Silicon, AWS Graviton, Oracle Cloud ARM).
- **SIMD Support**:
  - SSE2: Baseline para hardware legado.
  - SSE4.1/4.2: Otimizações de hashing.
  - AVX2: Kernel principal para processadores modernos.
  - AVX512: Performance máxima para Xeon/Epyc.
  - ARM NEON: Performance otimizada para infraestrutura cloud ARM.

## Dependencies
- **Standard Library (STL)**: Foco em containers de alta performance e threading.
- **No External Crypto Libs**: Implementação customizada de Secp256k1, SHA256 e RIPEMD160 para evitar overhead e garantir controle total sobre o pipeline SIMD.