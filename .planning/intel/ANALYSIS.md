# Análise de Código - Bchaves

**Data da Análise:** 2026-05-01  
**Projetista:** Carlos (GSD Codebase Mapper)  
**Linguagem:** Português (BR)

---

## Sumário Executivo

**Bchaves** é um motor de busca de alta performance para chaves privadas Bitcoin, desenvolvido inteiramente em C++17 nativo. O sistema implementa três algoritmos de busca (Address/BSGS/Kangaroo) com otimizações de baixo nível: endomorfismo GLV, SIMD AVX2, e filtros probabilísticos O(1).

Este documento fornece uma análise completa da arquitetura do sistema.

---

## Stack Tecnológico

### Linguagem Principal
- **C++17** - Toda a base de código
- **Zero dependências externas** - Apenas STL e libc

### Compilação
- **g++** (GCC 9+)
- **Flags:** `-std=c++17 -O3 -flto -Wall -Wextra`
- **Cross-platform:** x86_64 (native/SSE2) + ARM64 (Apple Silicon, AWS Graviton)

### Bibliotecas
- `<thread>`, `<mutex>`, `<atomic>` - Concorrência
- `<filesystem>` - I/O de arquivos
- `<chrono>` - Medição de tempo
- Nenhuma dependência externa

---

## Estrutura de Diretórios

```
Bchaves/
├── core/              # Camada criptográfica nativa
│   ├── secp256k1.cpp # Aritmética de curva elíptica
│   ├── hash.cpp      # SHA-256 + RIPEMD-160
│   ├── base58.cpp   # Codificação Base58Check
│   ├── address.cpp  # Derivação de endereços BTC
│   └── cuckoo.hpp  # Filtro probabilístico O(1)
├── engine/           # Motores de busca
│   ├── address.cpp # Busca linear/híbrida
│   ├── bsgs.cpp  # Baby-Step Giant-Step
│   └── kangaroo.cpp # Pollard's Kangaroo (fleet 64)
├── system/          # Camada de sistema
│   ├── cli.cpp    # Parsing de argumentos
│   ├── hardware.cpp # Detecção de CPU/RAM
│   ├── checkpoint.cpp # Persistência
│   └── targets.cpp # Carregador de alvos
├── modulos/        # Entry points
├── tests/         # Testes unitários
└── puzzles/       # Alvos de busca (puzzles Bitcoin)
```

---

## Componentes e Responsabilidades

| Componente | Arquivo | Função |
|-----------|---------|--------|
| **CLI Parser** | `system/cli.cpp` | Parse `-b`, `-k`, `-A`, `-R` |
| **Hardware Detector** | `system/hardware.cpp` | Detecta AVX2, BMI2, RAM |
| **Auto-Tuner** | `system/hardware.cpp:tune_for()` | Configura threads/batch |
| **Target Loader** | `system/targets.cpp` | Carrega endereços/pubkeys |
| **Address Engine** | `engine/address.cpp` | Busca por bit-range |
| **BSGS Engine** | `engine/bsgs.cpp` | BSGS com Cuckoo |
| **Kangaroo Engine** | `engine/kangaroo.cpp` | Pollard's Kangaroo |
| **Checkpoint** | `system/checkpoint.cpp` | Persistência/retomada |
| **Secp256k1** | `core/secp256k1.cpp` | Aritmética de curva |
| **Hash Layer** | `core/hash.cpp` | SHA-256/RIPEMD-160 |
| **Address Deriver** | `core/address.cpp` | Deriva endereços BTC |
| **Cuckoo Filter** | `core/cuckoo.hpp` | Lookup O(1) |

---

## Fluxos de Dados

### Fluxo Principal: Address Mode

```
1. CLI Input
   → parse_address_cli()
   
2. Carregar Targets
   → load_targets() → AddressMatcher (hash160s + CuckooFilter)

3. Setup Workers
   → detect_hardware() → tune_for() → num_threads

4. Processing Loop (por thread)
   → secp256k1_multiply() [gerar pontos]
   → batch_normalize()    [Jacobian → Affine]
   → SHA-256 + RIPEMD-160 [hash pubkey]
   → CuckooFilter.lookup() [O(1) pre-check]
   → binary_search()     [verificação]

5. Match Found
   → derive_key_info() [gerar endereços]
   → report_found() [stdout + found.txt]

6. Checkpoint (a cada 300s)
   → save_checkpoint()
```

### Fluxo BSGS

```
1. Baby Steps (construir tabela)
   → Para i = 1..m: gi = i*G, insert(xi) em CuckooFilter
   
2. Giant Steps (busca)
   → Para j = 0..n: gj = target - j*m*G
   → Se filter.lookup(gj.x): binary_search na shard table
   
3. Resolve
   → candidate = j*m + i (se encontrado)
```

### Fluxo Kangaroo

```
1. Cold Boot (se existente)
   → load_traps_from_disk() → shards[64] + filter

2. Fleet Init (64 kangaroos por thread)
   → wild: start = target, tame: start = range_end

3. Jump Loop
   → x = current.x % 64 → jump[jump_idx] → p += jump

4. Trap Check
   → Se is_distinguished(x):
     → filter.lookup() (RAM pre-check)
     → shard.table.lookup() (RAM)
     → lookup_trap_on_disk() (fallback)
```

---

## Principais Abstrações

### BigInt (inteiro de 256 bits)
```cpp
struct BigInt {
    std::array<std::uint64_t, 4> limbs{};
};
```

### PointJacobian (forma projetiva)
```cpp
struct PointJacobian {
    BigInt x, y, z;  // z = 1 caso affine
};
```

### Secp256k1Point (forma afim)
```cpp
struct Secp256k1Point {
    BigInt x, y;
    bool infinity;
};
```

### CuckooFilter (lookup O(1))
```cpp
class CuckooFilter {
    bool insert(uint64_t hash);
    bool lookup(uint64_t hash) const;
    // ~2 bytes por entrada
};
```

### AddressMatcher
```cpp
struct AddressMatcher {
    std::vector<std::array<uint8_t, 20>> hashes;  // targets
    std::unique_ptr<CuckooFilter> filter;     // O(1) lookup
};
```

---

## Integrações Externas

### CPU Features Detectadas
| Feature | Uso |
|---------|-----|
| **AVX2** | SIMD: 8x SHA/RIPEMD por ciclo |
| **SSE4** | Otimizações bitwise |
| **BMI2** | Bit manipulation |
| **SHA-NI** | Hardware SHA |
| **NEON** (ARM) | SIMD ARM |

### Sistema Operacional
- **Linux** (qualquer distro moderna)
- **macOS** (Apple Silicon + Intel)
- **Windows** (via WSL ou MinGW)

### Nenhuma Dependência Externa
O projeto é **autossuficiente** - toda a criptografia (Secp256k1, SHA-256, RIPEMD-160, Base58) é implementada nativamente.

---

##makefile Estrutura de Build

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

# Build targets
make address  # → build/address
make bsgs     # → build/bsgs
make kangaroo # → build/kangaroo
make test     # → build/crypto_test
```

---

## Conclusões

### Pontos Fortes
1. **Zero dependências externas** - Fácil de build e deploy
2. **Código nativo de alto desempenho** - Otimizações de baixo nível
3. **Arquitetura modular** - Camadas bem separadas
4. **Checkpoint robusto** - Retomada exata
5. **Multi-plataforma** - x86_64 + ARM64

### Áreas de Atenção
1. **BSGS limitado a 126 bits** - Limite atual de indexação
2. **Sem GPU** - Apenas CPU
3. **Sem distributed computing** - single-node

### Padrões Observados
- Pipeline: CLI → Load → Workers → Process → Match → Report
- Concorrência: thread-per-core com pinning
- Batch processing: N itens → processar → normalizar
- Checkpoint: binário versionado

---

*Documento gerado pelo GSD Codebase Mapper em 2026-05-01.*