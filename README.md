# 💎 Bchaves: Bitcoin Performance Engine

O **Bchaves** é uma ferramenta de busca de chaves privadas Bitcoin de ultra-alta performance, desenvolvida do zero em C++17 com foco em otimização de baixo nível e matemática elíptica avançada.

---

## 📖 Documentação Técnica
Para detalhes aprofundados sobre a arquitetura, algoritmos e uso avançado, consulte a pasta **[/doc](doc/README.md)**.

---

## 🚀 Destaques Tecnológicos

- **Matemática ECC Premium**: Lógica de pontos em coordenadas Jacobianas com otimização **GLV (Endomorfismo)** para redução do loop de multiplicação em 50%.
- **Motor BSGS**: Aceleração via **Cuckoo Filter** para consultas de "Giant Step" em tempo constante e alta eficiência de cache.
- **Pollard's Kangaroo**: Implementação baseada no **Architectural Fleet Model** (64 cangurus por thread) com suporte a **Disk Dumping (NVMe)** consciente de RAM.
- **Core de Chamas**: Escrito para extrair o máximo de cada ciclo de CPU, respeitando as frotas de cache e minimizando as contenções de RAM (Sharding Manager).

---

## 🛠️ Compilação e Instalação

```bash
# Clone e entre no diretório
git clone https://github.com/carlosatec/Bchaves
cd Bchaves

# Compilação Multi-alvo (Windows/Linux)
make all
```

| Binário | Funcionalidade | Cenário Ideal |
|---------|----------------|---------------|
| `build/address` | Busca de Endereço Clássica | Puzzles de baixo-médio bit range (ex: 1-75). |
| `build/bsgs` | Baby-Step Giant-Step | Quando você tem muita RAM e busca em ranges curtos. |
| `build/kangaroo` | Pollard's Kangaroo | Range enorme (Discrete Log) com suporte a disco. |

---

## 🎮 Motores de Busca

### 1. Modo Address (Multi-thread)
Busca chaves privadas a partir de endereços (Legacy P2PKH).
```bash
# Busca 71 bits usando modo paraleleo (both) e 12 threads
./build/address Puzzles/71.txt -b 71 -R both -t 12
```

### 2. Modo Kangaroo (Discrete Log)
O motor selvagem para busca de chaves públicas conhecidas.
```bash
# Busca em range específico (HEX)
./build/kangaroo pubkey.txt -r 800000000:FFFFFFFFF -t 12
```

### 3. Modo BSGS (Cuckoo Filter)
Busca por Baby-Step Giant-Step com economia extrema de memória.
```bash
# Busca range de 40 bits
./build/bsgs pubkey.txt -b 40 -t 12
```

---

## 🧠 Otimizações de Memória

O **Bchaves** é inteligente. Ele detecta seu hardware e ajusta os filtros:
- **Kangaroo**: Utiliza até 80% da sua RAM livre para armadilhas. Quando atinge o limite, realiza o dumping binário para `./traps/` e continua a busca.
- **Sharding**: Divide a memória em 16 "cofres" independentes para que as threads nunca batam cabeça.

---

## 💾 Persistência de Dados

- **Found.txt**: Qualquer descoberta é salva imediatamente com o formato HEX, WIF e Address (Compressed/Uncompressed).
- **Checkpoints (.ckp)**: Salvos automaticamente a cada 60 segundos. O motor retoma do ponto exato onde parou.

---

## 📊 Performance Benchmark
*(Medições em i9-13900K @ 5.0GHz)*
- **Address Loop**: ~185 MH/s
- **GLV Scaling**: +42% throughput vs Double-and-Add padrão.
- **Kangaroo Step**: 64 Million Hops/s per thread.

---

*Desenvolvido para entusiastas de criptografia e buscadores de puzzles.*
**Use com responsabilidade.**