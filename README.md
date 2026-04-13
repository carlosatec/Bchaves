# 💎 Bchaves: Bitcoin Performance Engine

O **Bchaves** é uma ferramenta de busca de chaves privadas Bitcoin de ultra-alta performance, desenvolvida do zero em C++17 com foco em otimização de baixo nível e matemática elíptica avançada.

---

## 📖 Documentação Técnica
Para detalhes aprofundados sobre a arquitetura, algoritmos e uso avançado, consulte a pasta **[/doc](doc/README.md)**.

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
| `build/address` | Busca de Endereço Clássica | Puzzles de baixo-médio bit range (ex: 1-130). Suporta **SegWit**. |
| `build/bsgs` | Baby-Step Giant-Step | Quando você busca em ranges curtos com alta densidade de Baby Steps. |
| `build/kangaroo` | Pollard's Kangaroo | Range enorme com suporte a pontos distinguidos e disco. |

---

## 🎮 Motores de Busca

### 1. Verificação de Hardware
Antes de começar, veja as capacidades da sua CPU:
```bash
./build/address --list-hardware
```

### 2. Modo Address (Multi-thread)
Busca chaves privadas a partir de endereços. Suporta **bc1**, **3...** e **1...**.
```bash
# Busca 71 bits usando modo paralelo (both) e 1threads
./build/address puzzles/71.txt -b 71 -R both -t 12
```

### 3. Modo Kangaroo (Discrete Log)
Agora com suporte simplificado por bits:
```bash
# Busca em range de 75 bits (calcula 2^74 até 2^75-1 automaticamente)
./build/kangaroo targets.txt -b 75 -t 12
```

### 4. Modo BSGS (Zero Lock Contention)
Utiliza busca binária em shards ordenados para eliminar travas entre threads.
```bash
./build/bsgs pubkey.txt -b 40 -t 12
```

---

## 🧠 Otimizações de Memória

- **Zero-Allocation**: Hot-loops redesenhados para evitar alocações de heap, minimizando pressão no GC e latência de cache.
- **Sharding Manager**: Divide a memória em 16 "cofres" independentes para eliminar contenção.

---

## 💾 Persistência de Dados

- **Found.txt**: Qualquer descoberta é salva imediatamente com HEX, WIF e Address conforme detectado.
- **Checkpoints de Emergência**: Ao pressionar `Ctrl+C`, o sistema tenta salvar o estado atual instantaneamente para retomada futura.

---

*Desenvolvido para entusiastas de criptografia e buscadores de puzzles.*
**Use com responsabilidade.**