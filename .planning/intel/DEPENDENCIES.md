# Dependências e Integrações

**Data da Análise:** 2026-05-01

## Visão Geral

O **Bchaves** é um projeto **autossuficiente** com zero dependências externas além da biblioteca padrão C++17. Toda a criptografia é implementada nativamente.

## Dependências do Sistema

### Standard Library (C++17)

| Cabeçalho | Uso | Módulo |
|-----------|-----|--------|
| `<vector>`, `<array>` | Containers | Todos |
| `<memory>` | smart pointers | engine/ |
| `<thread>`, `<mutex>` | Concorrência | engine/ |
| `<atomic>` | Variáveis atômicas | engine/ |
| `<chrono>` | Timers | engine/ |
| `<filesystem>` | File I/O | system/ |
| `<string>` | Manipulação de strings | core/ |
| `<cstdint>` | Tipos de tamanho fixo | Todos |
| `<csignal>` | Signal handling | engine/ |
| `<fstream>` | File streams | system/, engine/ |

### Libraries Externas

**Nenhuma.** O projeto não tem dependências de package managers (npm, pip, cargo, etc.).

## Dependências entre Módulos

### Estrutura de Dependências

```
modulos/ (entry points)
    │
    ├── address.cpp → engine::run_address()
    ├── bsgs.cpp   → engine::run_bsgs()
    └── kangaroo.cpp → engine::run_kangaroo()
            │
            ▼
        engine/ (motores)
            │
            ├── address.cpp ──► core/secp256k1.hpp
            │                 core/hash.hpp
            │                 core/cuckoo.hpp
            │                 system/checkpoint.hpp
            │                 system/hardware.hpp
            │                 system/targets.hpp
            │
            ├── bsgs.cpp ──────► core/secp256k1.hpp
            │                 core/cuckoo.hpp
            │                 core/hash.hpp
            │                 system/checkpoint.hpp
            │
            └── kangaroo.cpp ──► core/secp256k1.hpp
                                core/hash.hpp
                                core/cuckoo.hpp
                                core/hash_table.hpp
                                system/checkpoint.hpp
            │
            ▼
        core/ (criptografia)
            │
            ├── secp256k1.cpp ──► (standalone - math only)
            ├── hash.cpp ────────► (standalone)
            ├── base58.cpp ───────► (standalone)
            ├── address.cpp ──────► core/base58.hpp
            │                   core/hash.hpp
            └── cuckoo.hpp ─────► (inline header)
            │
            ▼
        system/ (infraestrutura)
            │
            ├── cli.cpp ────────► system/types.hpp
            ├── hardware.cpp ───► system/types.hpp
            ├── targets.cpp ──────► system/types.hpp
            ├── checkpoint.cpp ──► system/types.hpp
            ├── format.cpp ─────► system/types.hpp
            └── io.cpp ───────► system/types.hpp
```

## Integrações Externas

### Sistema Operacional

| Integração | Finalidade | API |
|------------|-----------|-----|
| **Threads** | Paralelismo | `std::thread`, `bchaves::system::pin_thread_to_core()` |
| **Arquivos** | Input/output | `<filesystem>`, `<fstream>` |
| **Sinais** | Interrupt (SIGINT) | `<csignal>` |
| **Tempo** | Benchmarks | `<chrono>` |

### CPU Features (detecção)

| Feature | Uso | Detecção |
|---------|-----|----------|
| **AVX2** | SIMD hashing (8x) | `system/hardware.cpp` |
| **SSE4** | Otimizações bitwise | CPUID |
| **BMI2** | Bit manipulation | CPUID |
| **SHA-NI** | Hardware SHA | CPUID |
| **NEON** (ARM) | SIMD | CPUID |

### GPU / CUDA

**Não integrado.** O foco atual é CPU-only com SIMD (AVX2/NEON).

### OpenCL / Vulkan

**Não integrado.** Implementação futura possível.

## Fluxo de Dados entre Camadas

### 1. CLI → Sistema

```
argv[] 
    │
    ▼ [parse_address_cli()]
system::cli.cpp
    │
    ▼ [AddressOptions]
system::targets.cpp
    │
    ▼ [TargetLoadResult]
engine::run_address()
```

### 2. Sistema → Engine

```
engine::run_address()
    │
    ▼ detect_hardware()
system::hardware.cpp (detecta CPU/RAM)
    │
    ▼ tune_for()
system::hardware.cpp (auto-tune)
    │
    ▼ worker loop
engine::address.cpp (run_hybrid_worker)
```

### 3. Engine → Core

```
run_hybrid_worker()
    │
    ▼ secp256k1_multiply()
core/secp256k1.cpp
    │
    ▼ SHA-256 / RIPEMD-160
core/hash.cpp
    │
    ▼ derive_key_info()
core/address.cpp (address derivation)
    │
    ▼ reporte
system/io.cpp (found.txt)
```

### 4. Checkpoint

```
engine/XXX.cpp
    │
    ▼ save_checkpoint()
system/checkpoint.cpp
    │
    ▼ load_checkpoint()
system/checkpoint.cpp
    │
    ▼ (retomar estado)
engine/XXX.cpp
```

## Key Interfaces

### engine::run_address()
```cpp
int run_address(const bchaves::system::AddressOptions& options);
// Entrada: AddressOptions (target_path, bits, mode, chunk_k, ...)
// Saída: 0 (encontrado) ou 1 (não encontrado/erro)
```

### core::secp256k1_multiply()
```cpp
Secp256k1Point secp256k1_multiply(const BigInt& scalar);
// Input: Private key (BigInt)
// Output: Public key point (Secp256k1Point)
```

### core::sha256()
```cpp
std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len);
// Input: Dados arbitrários
// Output: SHA-256 hash (32 bytes)
```

### core::ripemd160()
```cpp
std::array<std::uint8_t, 20> ripemd160(const std::uint8_t* data, std::size_t len);
// Input: SHA-256 hash (32 bytes)
// Output: RIPEMD-160 hash (20 bytes)
```

### system::load_targets()
```cpp
TargetLoadResult load_targets(const std::filesystem::path& file, bool require_pubkeys_only);
// Input: Arquivo de alvos
// Output: TargetLoadResult { entries, warnings }
```

## Makefile Dependencies

```makefile
ENGINE_SOURCES = \
    engine/address.cpp \
    engine/bsgs.cpp \
    engine/kangaroo.cpp \
    engine/app.cpp

COMMON_SOURCES = \
    core/address.cpp \
    core/base58.cpp \
    core/hash.cpp \
    core/secp256k1.cpp \
    system/checkpoint.cpp \
    system/cli.cpp \
    system/format.cpp \
    system/hardware.cpp \
    system/targets.cpp \
    system/io.cpp
```

**Ordem de compilação:**
1. `COMMON_SOURCES` sempre compilados antes de `ENGINE_SOURCES`
2. `modulos/XXX.cpp` compilado por último (entry point)

---

*Análise de dependências: 2026-05-01*