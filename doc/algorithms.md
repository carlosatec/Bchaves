# 🧠 Algoritmos e Otimizações

O **Bchaves** implementa os algoritmos mais eficientes para o Problema do Logaritmo Discreto em Curvas Elípticas (ECDLP).

---

## 🚀 0. Hybrid Chunk Search (Novo)
O motor `hybrid` é o principal explorador de ranges de bits do Bchaves. Ele utiliza uma partição dinâmica que combina velocidade e cobertura estatística.

### Bijeção LCG (Linear Congruential Generator)
Diferente de buscas puramente randômicas que podem revisitar a mesma chave, o Bchaves utiliza uma bijeção matemática:
1.  **Divisão de Range**: O range total ($2^{bits}$) é dividido em $N$ chunks de tamanho fixo (definido por `-k`).
2.  **Passo Coprimo**: O sistema gera um passo $S$ que é coprimo ao total de chunks $N$ ($gcd(S, N) = 1$).
3.  **Bijeção Pura (Exclusivo Bchaves)**: Diferente de implementações baseadas em projetos legados que aplicavam hashes (como SplitMix64) sobre o passo LCG, o Bchaves utiliza a fórmula pura $(i \times S) \mod N$.
    -   **Correção Crítica**: Removemos a distorção matemática que causava colisões e repetição de chunks. 
    -   Isso garante que **cada chunk seja visitado exatamente uma vez** com 100% de cobertura determinística.
    -   A exploração é pseudoaleatória mas matematicamente perfeita, garantindo que nenhum Keys/s seja desperdiçado em chaves repetidas.

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

### Architectural Fleet Model (Ultra-RAM)
O Bchaves utiliza um modelo de "frota" onde cada thread gerencia 64 "cangurus" simultaneamente com alta persistência.
- **Distinguished Points:** Identificação de colisões entre cangurus selvagens e domesticados via bits de paridade.
- **Persistent Trap Merge:** Quando a RAM atinge o limite, as armadilhas são mescladas (merge) nos arquivos `traps/shard_X.bin`. Diferente de outros motores, o Bchaves não apaga o histórico anterior, garantindo que o progresso de meses de busca seja preservado.
- **Disk-Lookup Collision:** Se um canguru atinge um ponto identificado pelo Cuckoo Filter que não está na RAM, o sistema realiza uma busca binária no disco para validar a colisão. Isso permite encontrar a chave mesmo que a armadilha correspondente tenha sido salva há semanas.

## 3. Otimização GLV (Endo Fusion)
Implementada no `core/secp256k1.cpp` e fundida no motor `address.cpp`.
- **O que faz:** Aproveita o automorfismo elíptico para calcular pontos relacionados $P_1 = \beta X$, $P_2 = \beta^2 X$.
- **Endo Fusion:** No motor `address`, para cada ponto elíptico calculado, o sistema verifica automaticamente as chaves $k$, $\lambda k$ e $\lambda^2 k$.
- **Resultado:** O throughput é efetivamente **triplicado** (3 chaves verificadas por "preço" de uma), pois os pontos $P_1$ e $P_2$ são obtidos via multiplicações modulares de campo, muito mais baratas que somas de curvas elípticas.
- **Impacto:** Aumento massivo de MH/s sem aumento proporcional no consumo de energia.

## 4. Coordenadas Jacobianas e Batch Normalization
Todas as operações de ponto (`secp256k1_add`, `secp256k1_double`) são realizadas em coordenadas Jacobianas $(X, Y, Z)$.

### Batch Processing
O sistema processa chaves em lotes (normalmente 256 ou 1024):
- **Otimização de Inversão**: A normalização de Jacobiano para Afim exige uma inversão modular elíptica, que é extremamente cara.
- **Batch Normalize**: Para um lote de $B$ pontos, o Bchaves realiza $3B$ multiplicações e **apenas 1 inversão modular total**.
- **Resultado**: Ganho de até 10x na velocidade de hash comparado ao processamento ponto-a-ponto.
