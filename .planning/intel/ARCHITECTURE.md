# Arquitetura do Sistema

**Data da Análise:** 2026-05-01

## Visão Geral do Sistema

```
┌────────────────────────────────────────────────────────────────┐
│                      ENTRADA CLI                              │
│         ./build/address targets.txt -b 71 -R hybrid           │
└──────────────────────────┬─────────────────────────────────────┘
                         ▼
┌────────────────────────────────────────────────────────────────┐
│                system::cli (Parsing)                               │
│           system/targets.cpp (Carregar alvos)                  │
└──────────────────────────┬─────────────────────────────────────┘
                         ▼
┌────────────────────────────────────────────────────────────────┐
│              engine::run_<mode> (Orquestração)                  │
│     engine/address.cpp | engine/bsgs.cpp | engine/kangaroo.cpp │
└──────────────────────────┬─────────────────────────────────────┘
                         ▼
┌────────────────────────────────────────────────────────────────┐
│                bchaves::core (Camada Crypto)                   │
│    secp256k1.cpp │ hash.cpp │ base58.cpp │ address.cpp           │
│         BigInt │ SHA-256/RIPEMD-160 │ Base58Check               │
└──────────────────────────┬─────────────────────────────────────┘
                         ▼
┌────────────────────────────────────────────────────────────────┐
│                 SAÍDA (stdout + found.txt)                      │
└────────────────────────────────────────────────────────────────┘
```

## Componentes Principais

| Componente | Responsabilidade | Arquivo Principal |
|-----------|----------------|---------------|
| **CLI Parser** | Parse argumentos, valida inputs | `system/cli.cpp` |
| **Target Loader** | Carrega endereços/chaves do arquivo | `system/targets.cpp` |
| **Hardware Detector** | Detecta CPU, RAM, features | `system/hardware.cpp` |
| **Auto-Tuner** | Configura threads, batch size | `system/hardware.cpp:tune_for()` |
| **Address Engine** | Busca por bit-range | `engine/address.cpp` |
| **BSGS Engine** | Baby-Step Giant-Step | `engine/bsgs.cpp` |
| **Kangaroo Engine** | Pollard's Kangaroo | `engine/kangaroo.cpp` |
| **Checkpoint** | Persistência/retomada | `system/checkpoint.cpp` |
| **Secp256k1** | Aritmética de curva | `core/secp256k1.cpp` |
| **Hash Layer** | SHA-256 + RIPEMD-160 | `core/hash.cpp` |
| **Address Deriver** | Deriva endereços BTC | `core/address.cpp` |
| **Cuckoo Filter** | Lookup O(1) | `core/cuckoo.hpp` |

## Padrões arquiteturais

### 1. Pipeline de Processamento
**Padrão:** Each engine segue o mesmo pipeline:
```
Input → Load Targets → Init Workers → Process Batch → Check Match → Report/Checkpoint
```

### 2. Concorrência via Threads C++17
**Padrão:**
```cpp
std::vector<std::thread> workers;
for (uint32_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([=]() { worker(i, ...); });
}
for (auto& t : workers) t.join();
```

### 3. Batch Processing
**Padrão:**
```cpp
// Acumular N itens → Processar em massa → Normalizar → Verificar
batch_normalize(batch_p, batch_affine, valid_batch_size);
```

### 4. Pinning de Threads
**Padrão:** Cada thread é fixada a um core específico:
```cpp
bchaves::system::pin_thread_to_core(i);
```

## Fluxo de Dados

### Address Engine (Modo Híbrido)

```cpp
1. Carregar targets (AddressMatcher)
   └── system::load_targets() → CuckooFilter + vector<hash160>

2. Calcular chunks (LCG bijetor)
   └── g_hybrid_chunk_size × g_hybrid_total_chunks

3. Workers-processamento
   └── run_hybrid_worker() por thread

   3.1. Para cada chunk:
       - Gerar pontos (secp256k1_multiply)
       - Batch add_points (Jacobian)
       - batch_normalize
   
   3.2. Hashing:
       - SHA-256 (pubkey)
       - RIPEMD-160 (sha_result)
   
   3.3. Matching:
       - CuckooFilter.lookup() (O(1) pre-check)
       - binary_search (verificação completa)
   
   3.4. Se encontrado:
       - derive_key_info()
       - report_found()

4. Checkpoint (a cada 300s)
   └── system::save_checkpoint()
```

### BSGS Engine

```c++
1. Baby Steps (construir tabela)
   └── Para i = 1..num_baby_steps:
       - ger[i] = i * G
       - filter.insert(x-coordinate)
       - shards[shard_idx].table.push_back()

2. Giant Steps (busca)
   └── Para j = 0..max_giant_steps:
       - giant[j] = target_y - j * step_size * G
       - filter.lookup(x-coordinate)
       - binary_search na shard table
       - se encontrado: candidate = j*m + i
```

### Kangaroo Engine (Fleet Model)

```c++
1. Cold Boot (se existente)
   └── load_traps_from_disk() → shards[] + filter

2. Fleet Init (64 kangaroos por thread)
   └── Para cada thread:
       - wild: start = target_y
       - tame: start = range_end

3. Jump Loop
   └── Para cada kangaroo no fleet:
       - jump_idx = x % 64
       - p = p + jump_table[jump_idx]
       - distance += jump_table[jump_idx]
       
4. Trap Check
   └── Se is_distinguished(x):
       - lookup no filter
       - lookup no shard.table (RAM)
       - lookup_trap_on_disk (fallback)
       - se matching: candidate = resolved
```

## Camadas e Dependências

### Sistema → Core
```
system/cli.cpp     → system/types.hpp → core/secp256k1.hpp
system/hardware.cpp→ system/types.hpp
system/targets.cpp→ system/types.hpp, core/address.hpp
```

### Engine → Core
```
engine/app.cpp    → core/secp256k1.hpp
engine/address.cpp→ core/secp256k1.hpp, core/hash.hpp, core/cuckoo.hpp
engine/bsgs.cpp   → core/secp256k1.hpp, core/hash.hpp, core/cuckoo.hpp
engine/kangaroo.cpp→ core/secp256k1.hpp, core/hash.hpp, core/cuckoo.hpp
```

### Core (Dependências Internas)
```
secp256k1.cpp    → (standalone - sem dependências externas)
hash.cpp          → (standalone)
base58.cpp        → (standalone)
address.cpp       → hash.cpp, base58.hpp
cuckoo.hpp       → (inline header only)
```

## Key Abstractions

### BigInt (256-bit integer)
```cpp
struct BigInt {
    std::array<std::uint64_t, 4> limbs{};
};
```
Usado para: chaves privadas, coordenadas de pontos

### PointJacobian (affine → jacobian)
```cpp
struct PointJacobian {
    BigInt x, y, z;
};
```
Usado para: operações de adição em batch

### Secp256k1Point (affine form)
```cpp
struct Secp256k1Point {
    BigInt x, y;
    bool infinity;
};
```
Usado para: coordenadas de saída

### CuckooFilter
```cpp
class CuckooFilter {
    bool insert(uint64_t hash);
    bool lookup(uint64_t hash) const;
};
```
Usado para:Lookup O(1) em alvos/armadilhas

### AddressMatcher
```cpp
struct AddressMatcher {
    std::vector<std::array<std::uint8_t, 20>> hashes;
    std::unique_ptr<CuckooFilter> filter;
};
```
Usado para: Matching de endereços

## Pontos de Entrada

### Módulo Address
- **Entrada:** `./build/address <targets.txt> -b <bits> -R <mode> -k <chunk_k>`
- **Função:** `engine::run_address(AddressOptions)`
- **Targets:** Endereços Bitcoin (1..., 3..., bc1...) ou Hash160

### Módulo BSGS
- **Entrada:** `./build/bsgs <pubkey.txt> -b <bits> -k <table_k>`
- **Função:** `engine::run_bsgs(BsgsOptions)`
- **Targets:** Public keys (hex)

### Módulo Kangaroo
- **Entrada:** `./build/kangaroo <pubkey.txt> -r <range> -A <profile>`
- **Função:** `engine::run_kangaroo(KangarooOptions)`
- **Targets:** Public keys (hex)

## Checkpoint System

**Formato:** Binário, versionado (v5, v6)

**Campos:**
- `format_version`
- `algorithm` (address-sequential, address-hybrid, bsgs, kangaroo)
- `range_start`, `range_end`
- `current` / `worker_currents`
- `progress_primary`, `progress_secondary`
- `timestamp`

---

*Análise de arquitetura: 2026-05-01*