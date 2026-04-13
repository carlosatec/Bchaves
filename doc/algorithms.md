# 🧠 Algoritmos e Otimizações

O **Bchaves** implementa os algoritmos mais eficientes para o Problema do Logaritmo Discreto em Curvas Elípticas (ECDLP).

## 1. Baby-Step Giant-Step (BSGS)
O BSGS é um algoritmo de *space-time tradeoff*. Para uma busca de $2^n$ chaves, ele gera $\sqrt{2^n}$ "baby steps" e realiza saltos gigantes de mesma magnitude.

### Aceleração BSGS Flat Memory
O Bchaves utiliza uma arquitetura de armazenamento **Flat** otimizada para densidade massiva:
- **Zero-Allocation**: Todos os buffers de busca são pré-alocados ou utilizam a pilha (stack), eliminando latência de heap allocation no loop quente.
- **Ordered Shards**: As Baby Steps são distribuídas em 16 shards independentes baseados nos bits menos significativos do HashX.
- **Busca Binária**: Cada shard é ordenado e consultado via `std::lower_bound`. Isso reduz o consumo de RAM de ~100 bytes (std::map) para apenas **16 bytes por entrada**.
- **Cuckoo Filter**: Atua como barreira probabilística ultra-rápida, evitando 99.9% das buscas desnecessárias nos shards.

## 2. Pollard's Kangaroo
Usado quando o alvo é uma Public Key conhecida e o range de busca é limitado (ou muito grande).

### Architectural Fleet Model
O Bchaves utiliza um modelo de "frota" onde cada thread gerencia 64 "cangurus" simultaneamente.
- **Distinguished Points:** O sistema usa pontos distintos para identificar colisões entre cangurus selvagens e domesticados.
- **Disk Dumping (NVMe):** Quando a RAM atinge 80% de uso, o sistema despeja as "armadilhas" (traps) no diretório `traps/`. Isso permite que a busca continue por dias ou meses sem estourar a memória.

## 3. Otimização GLV (Endomorfismo)
Implementada no `core/secp256k1.cpp`.
- **O que faz:** Aproveita um automorfismo eficiente na curva secp256k1 para decompor uma multiplicação escalar $k \cdot G$ em duas multiplicações menores.
- **Resultado:** Reduz o número de duplicas e adições de pontos pela metade (Aprox. 50%).
- **Impacto:** Aumento de ~40% no throughput real (MH/s).

## 4. Coordenadas Jacobianas
Todas as operações de ponto (`secp256k1_add`, `secp256k1_double`) são realizadas em coordenadas Jacobianas $(X, Y, Z)$.
- **Por que:** Evita inversões modulares dispendiosas durante o loop de soma. A inversão é feita apenas uma única vez ao final, para converter de volta para afim $(x, y)$.
