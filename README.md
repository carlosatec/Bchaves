# 💎 Bchaves: Bitcoin Performance Engine

O **Bchaves** é uma ferramenta de busca de chaves privadas Bitcoin de ultra-alta performance, desenvolvida do zero em C++17 com foco em otimização de baixo nível e matemática elíptica avançada.

---

## 🚀 Novidades na v5
- **Motor Hybrid Chunk**: Busca pseudoaleatória com cobertura de 100% via bijeção LCG.
- **Checkpoint v5**: Estado de persistência robusto e resumível sem perdas.
- **Matemática de 256 bits**: Divisões exatas para qualquer bit range.

---

## 🛠️ Compilação e Instalação

```bash
# Clone e entre no diretório
git clone https://github.com/carlosatec/Bchaves
cd Bchaves

# Compilação Multi-alvo (Windows/Linux)
make all
```

---

## 🎮 Motores de Busca

### 1. Address Mode (`build/address`)
O motor principal para exploração de puzzles por bits.

#### Modos de Exploração (`-R`)
| Modo | Descrição | Cenário Ideal |
|------|-----------|---------------|
| `hybrid` | **(Recomendado)** Explorador pseudoaleatório particionado. | Puzzle 30-160 |
| `sequential` | Busca linear incremental (`start` → `end`). | Ranges pequenos (< 30 bits) |
| `backward` | Busca linear decrescente (`end` → `start`). | Ranges pequenos |
| `both` | Busca bidirecional simultânea. | Verificação de extremidades |

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
./build/address targets.txt -b 50 -R hybrid -k 1024 --benchmark
```

---

### 2. Kangaroo Mode (`build/kangaroo`)
Baseado no algoritmo de Pollard's Kangaroo para Logaritmo Discreto.
```bash
# Busca em range de 75 bits (calcula 2^74 até 2^75-1 automaticamente)
./build/kangaroo targets.txt -b 75 -t 12
```

---

### 3. BSGS Mode (`build/bsgs`)
Modo Baby-Step Giant-Step com otimização de cache e busca binária.
```bash
./build/bsgs pubkey.txt -b 40 -t 12
```

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
- **`-l <tipo>`**: Filtro de compressão de endereço (`compress`, `uncompress`, `both`).
- **`-A <perfil>`**: Perfil de hardware (`safe`, `balanced`, `max`).

---

## 💾 Persistência e Checkpoints

- **Checkpoint v5**: Agora o checkpoint é atômico e salva o contador exato de chunks processados.
- **Resumo Automático**: Ao reiniciar uma busca interrompida, o Bchaves detecta o arquivo `.ckp` e retoma exatamente de onde parou.
- **FOUND.txt**: Descobertas são salvas em log com a Chave Privada em HEX.

---

*Desenvolvido para entusiastas de criptografia e buscadores de puzzles.*
**Use com responsabilidade.**