<!-- generated-by: gsd-doc-writer -->
# ⚙️ Configuration Guide

This document provides a detailed reference for all command-line parameters and configuration options available in **Bchaves**.

---

## 🚀 Common Parameters

These parameters are applicable across all search modules (`address`, `bsgs`, `kangaroo`).

### Range and Targets

| Flag | Description |
|------|-------------|
| `-b <bits>` | Bit range for search (1-256). For `address`: search space size. For `kangaroo`: auto-calculates range $[2^{b-1}, 2^b - 1]$. |
| `-r <start:end>` | Custom range in hexadecimal (Kangaroo only). Example: `-r 1:FFFFFFFF` |
| `<targets.txt>` | First positional argument: path to target file (addresses or public key hashes, one per line). |

### Performance Tuning

| Flag | Description |
|------|-------------|
| `-t <threads>` | Manually sets number of processing threads. If omitted, defaults to `-A` strategy. |
| `-A <profile>` | **Auto-Tune** profile: `safe` (50% physical cores), `balanced` (100% physical cores), `max` (all logical cores/Hyper-Threading). |
| `-k <multiplier>` | Chunk size multiplier (Address/BSGS): `1024` (~1M keys/batch), `4096` (~4M), `8192` (~8M). Higher = less ECC overhead for large ranges. |

### Persistence and Checkpoints

| Flag | Description |
|------|-------------|
| `-c <path>` | Explicit checkpoint file path. |
| `--checkpoint-interval <seconds>` | Interval between automatic checkpoint saves (default: varies by engine). |
| `--no-checkpoint` | Disables checkpoint saving entirely. |
| `--benchmark` | Runs without checkpoints or found.txt logging. Pure performance testing mode. |

### Backend and Optimizations

| Flag | Description |
|------|-------------|
| `--secp256k1-backend <auto\|portable>` | `auto`: use optimized kernel (AVX2/ARM Crypto). `portable`: force plain C++ implementation. |
| `--no-endo` | Disable GLV endomorphism optimization (Address mode). Useful for debugging. |
| `--list-hardware` | Display detected CPU features (vendor, cores, cache, ISA level, NUMA) and exit. |
| `-h`, `--help` | Display help message and exit. |

---

## 🔍 Hardware Detection (`--list-hardware`)

The `--list-hardware` flag displays comprehensive system information:

```bash
./build/address --list-hardware
```

**Output includes:**
- **CPU**: Vendor, family, and model (e.g., "GenuineIntel Core i7 12700K")
- **Cores**: Physical, logical, and SMT status
- **Cache**: L1d, L2, L3 sizes
- **Memory**: Total RAM, DDR generation, channel count
- **Features**: Detected SIMD instructions (SSSE3, SSE4, AVX, AVX2, AVX512, SHA-NI, BMI2, NEON)
- **ISA Level**: Highest available instruction set
- **NUMA**: Enabled or disabled
- **Recommended Tuning**: Threads, batch size, table K for balanced profile

---

## ⚡ ISA-Aware Auto-Tuning

The auto-tune system automatically adjusts batch size based on detected CPU capabilities:

### Batch Size Scaling by ISA

| ISA Level | Base Batch | ISA Multiplier | Notes |
|-----------|------------|----------------|-------|
| SSSE3 | 256 | 1x | Legacy baseline |
| SSE4 | 256 | 2x | - |
| AVX | 256 | 2x | 128-bit SIMD |
| AVX2 | 512 | 4x | 256-bit SIMD, 8 hashes/cycle |
| AVX512 | 512 | 8x | 512-bit SIMD |
| NEON | 256 | 2x | ARM64 128-bit |

### Profile Multipliers

| Profile | Thread Count | Batch Multiplier |
|---------|--------------|------------------|
| `safe` | 50% physical cores | 1x |
| `balanced` | 100% physical cores | 2x |
| `max` | 100% logical cores | 4x |

### CPU-Specific Adjustments
- **Xeon**: 2.0x multiplier (server-grade reliability)
- **Core i7/i9**: 1.5x multiplier
- **Ryzen**: 1.5x multiplier
- **Apple Silicon**: 2.0x multiplier

---

## 📍 Address Engine (`build/address`)

Search strategy for puzzle hunting and bit-range exploration.

### Usage Example
```bash
./build/address targets.txt -b 71 -R hybrid -k 4096 -A balanced
```

### Address-Specific Flags

| Flag | Description |
|------|-------------|
| `-R <mode>` | Search mode: `sequential` (linear), `hybrid` (recommended - partitioned pseudo-random with 100% coverage). |
| `-l <type>` | Address compression filter: `compress` (compressed WIF only), `uncompress` (uncompressed only), `both` (searches both, doubles effective speed). |

### Limitations
- `backward` and `both` modes are not yet implemented. Use `sequential` or `hybrid`.

---

## 📊 BSGS Engine (`build/bsgs`)

Baby-Step Giant-Step algorithm optimized for memory efficiency.

### Usage Example
```bash
./build/bsgs targets.txt -b 40 -k 8192 -A max
```

### BSGS-Specific Flags

| Flag | Description |
|------|-------------|
| `-k <size>` | Table size multiplier. Baby steps table = $1024 \times k$ entries. Higher = more memory but faster lookups. |
| `-l <type>` | Address compression filter: `compress`, `uncompress`, `both`. |

---

## 🦘 Kangaroo Engine (`build/kangaroo`)

Pollard's Kangaroo algorithm for large-range discrete logarithm problems.

### Usage Example
```bash
./build/kangaroo targets.txt -b 75 -A max --trap-dir my_traps/
```

### Kangaroo-Specific Flags

| Flag | Description |
|------|-------------|
| `--trap-dir <dir>` | Directory for persisted trap/jump points. Default: `traps/`. |
| `--wild <N>` | Percentage of wild kangaroos (0-100). Default: 50. Higher = more exploration. |
| `--tame <N>` | Percentage of tame kangaroos (0-100). Default: 50. Higher = more focused search. |
| `--no-load` | Skip loading existing traps from disk (cold boot). Useful for quick benchmarks. |

---

## 🔧 Build Optimization Flags

These flags are set at compile time via Makefile, not runtime:

| Flag | Description |
|------|-------------|
| `ARCH=sse2` | Build for legacy x86 (Westmere era, SSE2 only). Use for older CPUs. |
| Default (native) | Auto-detect CPU and enable all available instructions (AVX2, BMI2, etc.). |
| `-march=armv8.2-a+crypto` | ARM64 with cryptographic extensions (Apple Silicon, AWS Graviton). |

### Build Commands
```bash
make all          # Build all engines
make address      # Build address engine only
make bsgs         # Build BSGS engine only
make kangaroo     # Build Kangaroo engine only
make test         # Build and run cryptographic integrity tests
make clean        # Clean build artifacts
```

---

## 📋 Quick Reference

| Engine | Required Flags | Example |
|--------|---------------|---------|
| Address | `-b <bits>` | `./build/address targets.txt -b 71 -R hybrid` |
| BSGS | `-b <bits> -k <size>` | `./build/bsgs targets.txt -b 40 -k 8192` |
| Kangaroo | `-b <bits>` or `-r <start:end>` | `./build/kangaroo targets.txt -b 75` |

---

*Generated by GSD Documentation System.*