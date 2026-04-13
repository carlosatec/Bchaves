# 🧠 Algoritmos e Otimizações

O **Bchaves** implementa os algoritmos mais eficientes para o Problema do Logaritmo Discreto em Curvas Elípticas (ECDLP).

---

## 🚀 0. Hybrid Chunk Search (Novo)
O motor `hybrid` é o principal explorador de ranges de bits do Bchaves. Ele utiliza uma partição dinâmica que combina velocidade e cobertura estatística.

### Bijeção LCG (Linear Congruential Generator)
Diferente de buscas puramente randômicas que podem revisitar a mesma chave, o Bchaves utiliza uma bijeção matemática:
1.  **Divisão de Range**: O range total ($2^{bits}$) é dividido em $N$ chunks de tamanho fixo (definido por `-k`).
2.  **Passo Coprimo**: O sistema gera um passo $S$ que é coprimo ao total de chunks $N$ ($gcd(S, N) = 1$).
3.  **Permutação**: A thread $i$ visita o chunk $C = (i \times S) \mod N$.
    -   Isso garante que **cada chunk seja visitado exatamente uma vez** antes da repetição de qualquer bloco.
    -   A exploração é pseudoaleatória, evitando que todas as threads busquem na mesma região consecutivamente.

### Eficiência ECC por Chunk
Cada thread calcula o ponto inicial do chunk em coordenadas Jacobianas uma única vez através de `secp256k1_multiply()`. O restante do chunk (ex: 4 milhões de chaves) é processado via somas incrementais (`add_points_mixed`), reduzindo drasticamente o peso computacional por chave encontrada.

---
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

## 4. Coordenadas Jacobianas e Batch Normalization
Todas as operações de ponto (`secp256k1_add`, `secp256k1_double`) são realizadas em coordenadas Jacobianas $(X, Y, Z)$.

### Batch Processing
O sistema processa chaves em lotes (normalmente 256 ou 1024):
- **Otimização de Inversão**: A normalização de Jacobiano para Afim exige uma inversão modular elíptica, que é extremamente cara.
- **Batch Normalize**: Para um lote de $B$ pontos, o Bchaves realiza $3B$ multiplicações e **apenas 1 inversão modular total**.
- **Resultado**: Ganho de até 10x na velocidade de hash comparado ao processamento ponto-a-ponto.
