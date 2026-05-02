# Stack Tecnológico

**Data da Análise:** 2026-05-01

## Linguagens

**Primária:**
- **C++17** - Linguagem principal do motor de busca. Utilizada em todos os módulos de engine, core e system.

**Secundária:**
- **C** - Padrão para funções de sistema (syscall bindings)
- **Python** (scripts de build) - Makefile utiliza shell commands

## Runtime & Compilação

**Compilador Suportados:**
- **g++** (GCC) - Compilador principal configurado no Makefile
- **clang** - Suportado indiretamente via flag `-stdlib=libc++`
- Suporte a cross-compilation: **ARM64** (Apple Silicon, AWS Graviton)

**Flags de Compilação:**
```makefile
CXXFLAGS = -std=c++17 -O3 -flto -Wall -Wextra
```

**Otimizações de Arquitetura:**
-Arquitetura Padrão: `-march=native` (detecção automática de CPU)
- **SSE2**: `-march=westmere -msse2 -mno-avx` (modo portátil)
- **ARM64**: `-march=armv8.2-a+crypto` (Crypto extensions)

## Frameworks & Bibliotecas

### Bibliotecas C++ Padrão
- `<vector>`, `<array>`, `<memory>` - Containers STL
- `<thread>`, `<mutex>`, `<atomic>` - Concorrência C++17
- `<chrono>` - Medição de tempo
- `<filesystem>` - Operações de arquivo (C++17)

### Sem Dependências Externas
O projeto **não tem dependências externas** além da biblioteca padrão:
- **Secp256k1**: Implementação nativa em `core/secp256k1.cpp`
- **SHA-256/RIPEMD-160**: Implementação nativa em `core/hash.cpp`
- **Base58**: Implementação nativa em `core/base58.cpp`

### Ferramentas de Build
- **Make** - Sistema de build principal
- **Git** - Controle de versão

## Estrutura de Namespaces

**Namespace Principal:** `bchaves`

| Namespace | Propósito | Localização |
|-----------|----------|------------|
| `bchaves::core` | Primitive matemáticas, curvas, hashing | `core/*.cpp` |
| `bchaves::engine` | Motores de busca (Address, BSGS, Kangaroo) | `engine/*.cpp` |
| `bchaves::system` | CLI, hardware detection, checkpoint, I/O | `system/*.cpp` |

## Entrada e Saída

**Entrada:**
- Arquivo de alvos: `<path>` (endereços Bitcoin ou public keys)
- Argumentos CLI: `-b bits`, `-k chunk_k`, `-A profile`, etc.

**Saída:**
- Console: stdout com stats em tempo real
- Arquivo `found.txt`: Resultados encontrados
- Checkpoint files: Estado de progresso (`.ckp`)

## Requisitos de Sistema

**Desenvolvimento:**
- C++17-compliant compiler (g++ 9+ ou clang 10+)
- Make
- POSIX shell (Linux/macOS) ou WSL (Windows)

**Produção:**
- Linux (qualquer distribuição moderna)
- macOS (Apple Silicon ou Intel)
- Windows (via WSL ou MinGW)
- RAM: Mínimo 4GB ( Recomendado 8GB+)
- CPU: Mínimo 2 cores

---

*Análise do stack: 2026-05-01*