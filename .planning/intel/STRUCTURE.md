# Estrutura de Diretórios

**Data da Análise:** 2026-05-01

## Layout do Projeto

```
Bchaves/
├── .planning/           # Documentação de planejamento GSD
├── build/               # Binários compilados
├── core/               # Biblioteca criptográfica nativa
│   ├── secp256k1.cpp   # Aritmética de curva elíptica
│   ├── secp256k1.hpp   # Definições BigInt, Point, Jacobian
│   ├── hash.cpp        # SHA-256, RIPEMD-160
│   ├── hash.hpp        # Headers de hashing
│   ├── base58.cpp     # Codificação Base58
│   ├── base58.hpp     # Headers Base58
│   ├── address.cpp    # Derivação de endereços BTC
│   ├── address.hpp   # Headers de derivação
│   ├── cuckoo.hpp    # Cuckoo Filter (O(1) lookup)
│   └── cuckoo.cpp   # Implementação (inline header)
├── engine/             #Motores de busca
│   ├── app.cpp        # Orquestração global
│   ├── app.hpp       # Headers comuns
│   ├── address.cpp   # Motor Address (Puzzle search)
│   ├── bsgs.cpp    # Motor BSGS (Baby-Step Giant-Step)
│   └── kangaroo.cpp # Motor Kangaroo (Pollard's algorithm)
├── system/             #Camada de sistema
│   ├── cli.cpp        # Parsing de argumentos
│   ├── cli.hpp       # Headers CLI
│   ├── hardware.cpp # Detecção de CPU/RAM
│   ├── hardware.hpp # Auto-tune profiling
│   ├── checkpoint.cpp# Persistência de estado
│   ├── checkpoint.hpp# Headers checkpoint
│   ├── targets.cpp  # Carregador de alvos
│   ├── targets.hpp # Headers targets
│   ├── format.cpp   # Formatação de output
│   ├── format.hpp  # Headers format
│   ├── io.cpp      # I/O de arquivos
│   ├── io.hpp     # Headers I/O
│   └── types.hpp  # Tipos globais (enums, structs)
├── modulos/            #Templates de módulos (entry points)
│   ├── address.cpp   # Entry point: Address mode
│   ├── bsgs.cpp   # Entry point: BSGS mode
│   └── kangaroo.cpp# Entry point: Kangaroo mode
├── tests/             #Testes unitários
│   ├── crypto_test.cpp # Testes de integridade crypto
│   └── (outros arquivos de teste)
├── puzzles/            #Arquivos de puzzleBitcoin
│   ├── *.txt         # Alvos de busca (101-129)
├── traps/             #Armadilhas do Kangaroo (gerado em runtime)
├── doc/               # Documentação do projeto
├── Makefile          # Sistema de build
└── README.md         # Documentação principal
```

## Propósito dos Diretórios

### `core/`
**Propósito:** Biblioteca criptográfica nativa de baixo nível.

**Conteúdo:**
- Aritmética de curva elíptica Secp256k1
- BigInt (inteiros de 256-bit)
- Hashing (SHA-256, RIPEMD-160)
- Codificação (Base58Check)
- Cuckoo Filter para lookup O(1)

**Arquivos Principais:**
| Arquivo | Função |
|---------|--------|
| `secp256k1.cpp` | Multiplicação de pontos, aritmética modular |
| `hash.cpp` | SHA-256, RIPEMD-160 |
| `address.cpp` | Derivar endereços de chaves privadas |
| `cuckoo.hpp` | Filtro probabilístico |

### `engine/`
**Propósito:** Implementação dos algoritmos de busca.

**Conteúdo:**
- Address mode: Busca linear/híbrida por bit-range
- BSGS: Baby-Step Giant-Step
- Kangaroo: Pollard's Kangaroo (fleet model)

**Arquivos Principais:**
| Arquivo | Algoritmo |
|--------|----------|
| `address.cpp` | Range search sequencial + híbrido |
| `bsgs.cpp` | BSGS com Cuckoo Filter |
| `kangaroo.cpp` | Pollard's Kangaroo (64-fleet) |

### `system/`
**Propósito:** Camada de abstração do sistema.

**Conteúdo:**
- CLI parsing
- Detecção de hardware
- Checkpoint/resume
- Carregamento de alvos
- Output formatting

**Arquivos Principais:**
| Arquivo | Função |
|---------|--------|
| `cli.cpp` | Parse de argumentos `-b`, `-k`, `-A` |
| `hardware.cpp` | `detect_hardware()`, `tune_for()` |
| `checkpoint.cpp` | Persistência de estado |
| `targets.cpp` | Carregamento de alvos |

### `modulos/`
**Propósito:** Entry points dos motores.

**Padrão:**
Cada arquivo é um wrapper que chama o motor apropriado em `engine/` com opções parseadas da CLI.

### `tests/`
**Propósito:** Testes de validação.

**Principal:**
- `crypto_test.cpp`: Valida integridade da aritmética Secp256k1

### `puzzles/`
**Propósito:** Arquivos de alvos para поиска de puzzle.

**Formato:**
- Arquivos texto com endereços Bitcoin (1..., 3..., bc1...)
- Ou public keys em hex

### `traps/`
**Propósito:** Armadilhas persistidas do motor Kangaroo.

**Gerado em runtime:**
- Diretório criado automaticamente
- Contém arquivos `shard_*.bin` com armadilhas

## Conventions de Nomenclatura

### Arquivos
- **CamelCase**: `myClass.cpp`, `MyHeader.hpp`
- **Módulos**: `<modulo>.cpp` (entry point)
- **Engine**: `<algoritmo>.cpp` (implementação)

### Diretórios
- **lowercase**: `core/`, `engine/`, `system/`
- **Plural**: `tests/`, `puzzles/`, `traps/`

### Símbolos
- **Namespaces**: `bchaves::<modulo>`
- **Classes**: `CamelCase`
- **Funções**: `snake_case` ou `camelCase` (STL-style)
- **Constantes**: `kCamelCase` (prefixo: `k`)

## Onde Adicionar Novo Código

### Novo Módulo/Engine
1. **Implementação**: `engine/<algoritmo>.cpp`
2. **Header**: `engine/<algoritmo>.hpp` (ou usar `app.hpp`)
3. **Entry point**: `modulos/<algoritmo>.cpp`
4. **Build entry**: Adicionar no Makefile

### Nova Função Criptográfica
1. **Implementação**: `core/<função>.cpp`
2. **Header**: `core/<função>.hpp`

### Nova Utilidade de Sistema
1. **Implementação**: `system/<util>.cpp`
2. **Header**: `system/<util>.hpp`

### Novo Teste
1. **Local**: `tests/<nome>_test.cpp`
2. **Makefile**: Adicionar build target

---

*Análise de estrutura: 2026-05-01*