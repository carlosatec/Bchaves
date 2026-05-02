<!-- generated-by: gsd-doc-writer -->
# 💎 Bchaves: Bitcoin Performance Engine

**Bchaves** is an ultra-high performance Bitcoin private key search engine, developed from scratch in C++17 with a focus on low-level optimization and advanced elliptic curve mathematics.

---

## 🚀 Key Features

- **Hybrid-Endo Fusion Engine**: Native GLV endomorphism integration, tripling real throughput by processing 3 related keys per operation.
- **Optimized Field Arithmetic**: Multiplication and square kernels (`mod_mul_k1`, `mod_square_k1`) specifically tuned for the Secp256k1 prime.
- **ISA-Aware SIMD Acceleration**: Per-ISA optimized SHA-256 kernels (SSE4, AVX, AVX2, AVX512) with automatic detection and selection.
- **Phase 6 Hardware Detection**: Comprehensive CPU detection including brand, model, cache sizes (L1/L2/L3), NUMA topology, and ARM64 support.
- **v6 Checkpoint Protection**: Rigorous parameter validation to prevent progress corruption and ensure atomic resumption.
- **Large-Scale Cuckoo Filter**: O(1) probabilistic target matching, capable of managing billions of traps in RAM and on Disk.
- **Multi-Architecture Support**: Full compatibility with **modern ARM64 processors** (Apple Silicon, AWS Graviton) and x86_64.

---

## 🛠️ Build and Test

```bash
# Clone the repository
git clone https://github.com/carlosatec/Bchaves
cd Bchaves

# Build all search engines (address, bsgs, kangaroo)
make all

# Run cryptographic integrity validation
make test && ./build/crypto_test
```

---

## 🎮 Search Modules

### 1. Address Mode (`build/address`)
The primary engine for bit-range exploration (Puzzle search).
```bash
./build/address targets.txt -b 71 -R hybrid -k 4096 -A balanced
```

### 2. Kangaroo Mode (`build/kangaroo`)
Pollard's Kangaroo algorithm for large-range discrete logarithm problems.
```bash
./build/kangaroo targets.txt -b 75 -A max --trap-dir my_traps/
```

### 3. BSGS Mode (`build/bsgs`)
Baby-Step Giant-Step optimized for memory efficiency and lookup speed.
```bash
./build/bsgs pubkey.txt -b 40 -k 8192 -A max
```

---

## ⚙️ Hardware Auto-Tune (`-A`)

The **Auto-Tune** system automatically configures the engine based on your hardware and detected ISA level:

| Profile | Thread Count | Use Case |
|---------|--------------|----------|
| `safe` | 50% physical cores | Notebooks/Multitasking |
| `balanced` | 100% physical cores | Servers/Standard |
| `max` | 100% logical cores | Dedicated Rigs |

Batch size is automatically scaled by ISA level (SSE4=1x, AVX=2x, AVX2=4x, AVX512=8x).

---

## 🔍 Detect Your Hardware

View your system's detected CPU capabilities:

```bash
./build/address --list-hardware
```

This displays: CPU vendor/family/model, core counts, cache sizes, memory configuration, supported SIMD features (SSSE3, SSE4, AVX, AVX2, AVX512, SHA-NI, BMI2, NEON), ISA level, NUMA status, and recommended tuning parameters.

---

## 📚 Documentation

Detailed documentation is available in the `doc/` directory:

- [🚀 Getting Started](doc/GETTING-STARTED.md)
- [🏗️ System Architecture](doc/ARCHITECTURE.md)
- [⚙️ Configuration Reference](doc/CONFIGURATION.md)
- [🛠️ Development Guide](doc/DEVELOPMENT.md)
- [🧪 Testing and QA](doc/TESTING.md)

---

*Developed for cryptography enthusiasts and puzzle hunters.*
**Use responsibly.**