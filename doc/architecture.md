# 🏗️ Arquitetura do Sistema

O **Bchaves** utiliza uma arquitetura em camadas projetada para performance e modularidade. Abaixo está a explicação de cada diretório e seu papel no ecossistema.

## 📂 Estrutura de Diretórios

### 1. `core/` (Fundamentos Zero-Allocation)
Esta é a base do sistema, projetada para ter **zero alocações de heap** no loop crítico.
- **`secp256k1.cpp/hpp`**: Operações em curvas elípticas Jacobianas com otimização **GLV**. Funções de serialização redesenhadas para usar buffers locais.
- **`address.cpp/hpp`**: Lógica para derivação de endereços Bitcoin, agora suportando **SegWit (Bech32)** e **P2SH**.
- **`cuckoo.hpp`**: Filtro probabilístico de alta densidade.

### 2. `engine/` (Motores de Busca)
Contém a lógica pesada de "como encontrar a chave". É aqui que os algoritmos de busca são implementados.
- **`address.cpp`**: Motor de busca sequencial baseado em endereços públicos.
- **`bsgs.cpp`**: Algoritmo Baby-Step Giant-Step.
- **`kangaroo.cpp`**: Algoritmo Pollard's Kangaroo.
- **`app.hpp`**: Cabeçalho comum para estados globais do motor.

### 3. `modulos/` (Pontos de Entrada)
Arquivos pequenos que servem apenas como "wrappers" para criar binários diferentes.
- Cada arquivo contém um `main()` que parseia os argumentos da linha de comando e delega a execução para o motor correspondente na `engine/`.

### 4. `system/` (Utilidades do SO)
Gerenciamento de recursos do sistema e I/O.
- **`checkpoint.cpp`**: Persistência de progresso em arquivos `.ckp`.
- **`hardware.cpp`**: Detecção avançada de CPU (CPUID Leaf 4), identificando **Caches L3**, núcleos físicos e extensões de instrução (**AVX2, BMI2, SHA**).
- **`targets.cpp`**: Carregador de alvos polimórfico, tratando automaticamente endereços Legados, P2SH, SegWit e Hash160.

### 5. `traps/` e `puzzles/`
- **`traps/`**: Diretório usado pelo motor Kangaroo para despejar dados da memória no disco (NVMe) quando a RAM está cheia.
- **`puzzles/`**: Arquivos de configuração e alvos (hashes/endereços) para desafios específicos.

---

## 🔄 Fluxo de Dependências

```mermaid
graph TD
    M[modulos/bsgs.cpp] --> E[engine/bsgs.cpp]
    E --> C[core/secp256k1.hpp]
    E --> CF[core/cuckoo.hpp]
    E --> S[system/checkpoint.cpp]
    
    A[modulos/address.cpp] --> EA[engine/address.cpp]
    EA --> CA[core/address.hpp]
    EA --> CM[core/secp256k1.hpp]
```

## 🧩 Por que a separação?
A separação entre `core` e `engine` permite que o desenvolvedor otimize a matemática elíptica (no `core`) sem quebrar a lógica de multi-threading da busca (na `engine`). Além disso, facilita a criação de novos módulos de entrada sem duplicar código.
