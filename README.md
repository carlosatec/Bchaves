# 💎 Bchaves: Bitcoin Performance Engine

O **Bchaves** é uma ferramenta de busca de chaves privadas Bitcoin de ultra-alta performance, desenvolvida do zero em C++17 com foco em otimização de baixo nível e matemática elíptica avançada.

---

## 🚀 Novidades: Ultra Performance Update
- **Motor Hybrid-Endo Fusion**: Integração nativa de endomorfismo GLV, triplicando o throughput real ao processar 3 chaves relacionadas por operação.
- **Aritmética de Campo Otimizada**: Kernels de multiplicação e quadrado (`mod_mul_k1`, `mod_square_k1`) otimizados para o primo da Secp256k1.
- **Motor SIMD AVX2 Nativo**: Processamento de 8 hashes SHA-256 e RIPEMD-160 em paralelo por ciclo, agora integrado ao pipeline de endomorfismo.
- **Blindagem de Checkpoint v5**: Validação rigorosa de parâmetros (`-k`, `-R`) para evitar corrupção de progresso e garantir retomada atômica.
- **Suporte Multi-Formato Full**: Busca simultânea de endereços `compress`, `uncompress` e `both` com pipeline de paridade corrigida.
- **Filtro Cuckoo de Larga Escala**: Busca probabilística de alvos em O(1), agora dimensionado para gerenciar armadilhas de Kangaroo tanto em RAM quanto em Disco.
- **Correção da Bijeção LCG**: O motor `hybrid` agora utiliza matemática pura sem hashing distorcivo, garantindo 100% de cobertura real e eliminando chaves duplicadas (Bug do Legado corrigido).

---

## 🛠️ Compilação e Instalação

```bash
# Clone e entre no diretório
git clone https://github.com/carlosatec/Bchaves
cd Bchaves

# Compilação Multi-alvo (Windows/Linux)
make all

# Validação de Integridade Criptográfica (Opcional)
make test && ./build/crypto_test
```

---

## 🎮 Motores de Busca

### 1. Address Mode (`build/address`)
O motor principal para exploração de puzzles por bits.

#### Modos de Exploração (`-R`)
| Modo | Descrição | Cenário Ideal |
|------|-----------|---------------|
| `hybrid` | **(Recomendado)** Explorador pseudoaleatório particionado.
| `sequential` | Busca linear incremental (`start` → `end`). | Ranges pequenos (< 30 bits) |

#### Exemplo: Puzzle 71 com Modo Hybrid
```bash
./build/address puzzles/71.txt -b 71 -R hybrid -k 4096 -t 12
```
*   `-b 71`: Define o bit range do puzzle.
*   `-R hybrid`: Ativa o motor pseudoaleatório com cobertura total.
*   `-k 4096`: Multiplicador de chunk (4M chaves/bloco). Minimiza o custo ECC.
*   `-t 12`: Utiliza 12 threads de processamento.

#### Outros Exemplos
```bash
# Busca sequencial de 40 bits com endereços não-comprimidos
./build/address targets.txt -b 40 -R sequential -l uncompress

# Teste de throughput total (sem salvar arquivos)
./build/address targets.txt -b 50 -R hybrid -k 4096 --benchmark
```

---

### 2. Kangaroo Mode (`build/kangaroo`)
Baseado no algoritmo de Pollard's Kangaroo para Logaritmo Discreto.
```bash
# Busca em range de 75 bits (calcula 2^74 até 2^75-1 automaticamente)
./build/kangaroo targets.txt -b 75 -t 12 --trap-dir my_traps/

# Benchmark rápido (pula carregamento de disco e não salva novas armadilhas)
./build/kangaroo targets.txt -b 75 --benchmark --no-load
```

#### Parâmetros Específicos (Kangaroo)
- **`--trap-dir <caminho>`**: Local de armazenamento das armadilhas (Default: `traps/`).
- **`--wild <N>`**: Porcentagem de cangurus selvagens (Default: 50).
- **`--tame <N>`**: Porcentagem de cangurus domesticados (Default: 50).
- **`--no-load`**: Pula o carregamento inicial de armadilhas do disco (*Cold Boot*).

---

### 3. BSGS Mode (`build/bsgs`)
Modo Baby-Step Giant-Step com otimização de cache e busca binária.
```bash
# Busca em range de 40 bits com 12 threads e 4M Baby Steps (4096 * 1024)
./build/bsgs pubkey.txt -b 40 -t 12 -k 4096
```

#### Parâmetros Específicos (BSGS)
- **`-k <N>`**: Define o tamanho da tabela de *Baby Steps* (N * 1024). Maior valor usa mais RAM mas acelera a busca.
- **Identidade Forte**: O motor utiliza 256-bit `x` + paridade para eliminar colisões de busca.

---

## ⚙️ Perfis de Hardware (-A)

O parâmetro `-A` (Auto-Tune) ajusta automaticamente o número de threads e o tamanho dos lotes de processamento conforme o hardware detectado.

| Perfil | Estratégia | Cenário de Uso |
|--------|------------|----------------|
| `safe` | Metade dos núcleos físicos. | Notebooks, uso simultâneo com outras tarefas. |
| `balanced` | **(Padrão)** Todos os núcleos físicos. | Servidores compartilhados, equilíbrio térmica/speed. |
| `max` | Todos os núcleos lógicos (HT). | Rigs dedicadas, máxima performance possível. |

### Exemplos de Tuning por Módulo

```bash
# [Address] Uso leve para não travar o PC (Notebook)
./build/address target.txt -b 65 -R hybrid -A safe

# [Address] Força total em servidor dedicado (HT ativo)
./build/address target.txt -b 71 -R hybrid -A max

# [Kangaroo] Busca de 75 bits com perfil equilibrado (Fisico 100%)
./build/kangaroo targets.txt -b 75 -A balanced

# [BSGS] Busca em range de 40 bits usando perfil máximo
./build/bsgs pubkey.txt -b 40 -A max

# Override manual: Perfil max, mas limitando a 8 threads explicitamente
./build/address target.txt -b 71 -R hybrid -A max -t 8
```

---

## ⚙️ Parâmetros Técnicos (Address)

- **`-k <multiplicador>`**: Define o tamanho do bloco processado por thread.
    - `k=1024`: 1 milhão de chaves (Recomendado para puzzles pequenos 40-60 bits).
    - `k=4096`: 4 milhões de chaves (Recomendado para puzzles grandes 70+ bits).
    - `k=8192`: 8 milhões de chaves (Máxima diluição do custo ECC para puzzles 80+ bits).
- **Aceleração via -k**: Valores maiores de `-k` reduzem a frequência de cálculos pesados de curva elíptica na inicialização de cada chunk, aumentando a taxa líquida de Keys/s.
- **`-l <tipo>`**: Filtro de compressão de endereço (`compress`, `uncompress`, `both`).

---

## 📊 Telemetria e Performance

Ao rodar o motor, você verá duas métricas de velocidade:

- **`Keys/s` (Chaves Únicas)**: Representa o seu **progresso real** no range. Indica quantas chaves privadas distintas estão sendo percorridas por segundo. Use este número para estimar o tempo total da busca.
- **`Checks/s` (Total de Hashes)**: Representa o **esforço bruto** da CPU. Indica quantos endereços (hashes) estão sendo verificados por segundo.

### Por que os números são diferentes?
O motor utiliza multiplicadores de eficiência. Por exemplo, no modo `hybrid` com `-l both`:
- **Endomorfismo**: Triplica o trabalho (3 chaves relacionadas testadas por operação).
- **Modo Both**: Quadruplica o trabalho (Testa 2 variações comprimidas e 2 não-comprimidas por chave).
- **Resultado**: Cada 1 `Key/s` gera **12 `Checks/s`**.

- **`-A <perfil>`**: Perfil de hardware (`safe`, `balanced`, `max`).
- **`--secp256k1-backend <auto|portable>`**: Seleciona o kernel matemático.
    - `auto`: Escolhe a versão mais rápida disponível (ex: otimizada para x86_64).
    - `portable`: Força o uso da implementação C puro (seguro para ambientes instáveis).
- **`--no-endo`**: Desabilita a otimização de endomorfismo.
- **Aceleração de Hardware**: Detecta suporte a AVX2 e BMI2. O caminho SHA-NI está desabilitado por segurança até validação completa.
- **`--list-hardware`**: Exibe as features detectadas da CPU e encerra.

---

- **Checkpoint v6**: O `address` hybrid persiste chunks; o `address` sequencial agora persiste o proximo `current` de cada thread para retomada exata. `BSGS` continua com checkpoint por giant step global.
- **Controle de Caminho (`-c`)**: Use `-c <caminho>` para definir manualmente onde o checkpoint sera salvo. Semantica uniforme em todos os motores.
- **Armadilhas do Kangaroo (`--trap-dir`)**: Use `--trap-dir <diretorio>` para definir onde as armadilhas do `kangaroo` serao persistidas (default: `traps/`).
- **Resumo Automático**: Ao reiniciar uma busca interrompida, o `address` retoma do ultimo estado valido salvo. `BSGS` retoma do ultimo giant step persistido.
- **Kangaroo Ultra-Disk**: Busca de armadilhas no disco agora é O(log N), permitindo gerenciar bilhões de armadilhas sem perda de performance.
- **found.txt**: Descobertas são salvas em log formatado com a Chave Privada em HEX, WIF e Endereço.

---

*Desenvolvido para entusiastas de criptografia e buscadores de puzzles.*
**Use com responsabilidade.**
