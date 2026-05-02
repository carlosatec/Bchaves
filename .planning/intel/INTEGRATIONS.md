# Bchaves External Integrations

**Analysis Date:** 2026-05-01

## External Integrations Overview

Bchaves is designed as a **standalone system** with zero external dependencies. All cryptographic primitives are custom-implemented. This document covers the few external interfaces that exist.

---

## Input/Output

### File Input

**Targets File:**
- **File:** `targets.txt` (or specified via CLI)
- **Format:** One target per line
- **Supported Formats:**
  - Legacy addresses: `1...` (P2PKH)
  - P2SH addresses: `3...` (nested SegWit)
  - Bech32 addresses: `bc1...` (native SegWit)
  - Raw hash160: 40 hex characters (20 bytes)
  - Public keys: 64 or 130 hex characters (compressed/uncompressed)

**Example `targets.txt`:**
```
1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2
3D2GqSatLGuRRQCiewEMumX2wk5v4U3f9
bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4
0000000000000000000000000000000000000000
02d0c5437bdb514a51f4638c459567d9bae15e9ca4e03c09a4b7462d221de99a9a
```

### File Output

**Found Keys:**
- **File:** `found.txt`
- **Format:** Text, one finding per line with full context
- **Content:**
  ```
  Private Key (hex): <private_key_hex>
  Private Key (decimal): <private_key_decimal>
  WIF Compressed: <wif_comp>
  WIF Uncompressed: <wif_uncomp>
  Address (compressed): <addr_comp>
  Address (uncompressed): <addr_uncomp>
  Public Key (hex): <pubkey_hex>
  Mode: <search_mode>
  Thread: <thread_id>
  Time: <timestamp>
  ```

**Checkpoint Files:**
- **Pattern:** `*.ckp` (v6 format)
- **Naming:** `{algorithm}.ckp` or `{checkpoint_path}`
- **Format:** Binary serialized `CheckpointState`

---

## Command-Line Interface

Build binaries are self-contained and use no external libraries.

### Address Mode

```bash
./build/address <targets_file> [options]
```

| Flag | Description | Required |
|------|------------|----------|
| `<targets_file>` | Path to targets | Yes |
| `-b <bits>` | Bit range to search | No (default: auto) |
| `-R <mode>` | Search mode | No |
| `-k <num>` | Chunk size k | No |
| `-A <profile>` | Auto-tune | No |
| `-t <threads>` | Thread count | No |
| `-C <seconds>` | Checkpoint interval | No |
| `-c <file>` | Checkpoint file | No |
| `--no-checkpoint` | Disable checkpoint | No |
| `--benchmark` | Run benchmark | No |

**Search Modes:** `sequential`, `backward`, `both`, `hybrid`

**Auto-Tune Profiles:** `safe`, `balanced`, `max`

### BSGS Mode

```bash
./build/bsgs <targets_file> [options]
```

| Flag | Description |
|------|------------|
| `-b <bits>` | Bit range |
| `-k <table_k>` | Table size multiplier |

### Kangaroo Mode

```bash
./build/kangaroo <targets_file> [options]
```

| Flag | Description |
|------|------------|
| `-b <bits>` | Bit range |
| `--trap-dir <dir>` | Trap directory |
| `--no-load` | Skip trap loading |
| `--wild <N>` | Wild ratio % |
| `--tame <N>` | Tame ratio % |

---

## Directory Structures

### Trap Directory

Used by Kangaroo mode for persistent trap storage.

**Structure:**
```
traps/
├── meta.json       # Trap metadata
├── wild/         # Wild kangaroo seeds
│   ├── 00001.bin
│   └── ...
└── tame/        # Tame kangaroo seeds
    ├── 00001.bin
    └── ...
```

---

## Standard Streams

### Stdout

Progress output goes to standard output:
- Periodic status (configurable interval)
- Found key details
- Statistics

### Stderr

Error messages go to standard error:
- CLI parsing errors
- File I/O errors
- Checkpoint errors

### Signals

**Supported Signals:**
- `SIGINT` (Ctrl+C): Graceful stop with checkpoint save
- `SIGTERM`: Graceful stop

---

## Environment Variables

### Build-Time

| Variable | Description | Default |
|----------|-------------|---------|
| `CXX` | C++ compiler | `g++` |
| `ARCH` | Architecture | `native` |

**Usage:**
```bash
ARCH=sse2 make clean all   # SSE2 build
```

### Runtime

No runtime environment variables required.

---

## Platform-Specific Integrations

### CPU Features (Detected at Runtime)

| Feature | Detection | Usage |
|---------|----------|-------|
| AVX2 | CPUID | SHA-256 batch hashing |
| SHA-NI | CPUID | Hardware SHA |
| BMI2 | CPUID | Fast bit operations |
| ARM Crypto | CPUID | ARM64 optimizations |

### Memory Management

Uses standard C++ memory allocation:
- `new`/`delete` for objects
- `malloc`/`free` for large buffers
- Memory-mapped files for Cuckoo filter (disk-backed)

---

## No External Services

Bchaves does not connect to any external services:

| Service Type | Status |
|-------------|--------|
| Bitcoin nodes | Not used |
| Block explorers | Not used |
| APIs | Not used |
| Cloud services | Not used |
| Remote databases | Not used |

---

## Configuration Files

### Checkpoint File (v6)

Binary format, created automatically:

**Structure:**
```cpp
struct CheckpointState {
    uint32_t format_version = 5;
    char algorithm[32];
    SearchMode mode;
    SearchType type;
    uint32_t threads;
    uint32_t batch_size;
    uint64_t timestamp;
    uint8_t range_start[32];
    uint8_t range_end[32];
    uint8_t current[32];
    // v6 additions
    vector<uint8_t> worker_currents;
};
```

---

## Network

**No network connectivity**. Bchaves is an offline tool:
- No outgoing connections
- No incoming connections
- No DNS resolution
- No external API calls

---

## Security Context

No special security features required:
- No sandboxing
- No seccomp
- No AppArmor
- No SELinux profiles needed

---

## Summary

| Category | Integration |
|----------|------------|
| Input | File system (`targets.txt`) |
| Output | File system (`found.txt`, `*.ckp`) |
| Commands | CLI arguments only |
| Network | None |
| External deps | None |
| Cloud services | None |

Bchaves is entirely self-contained. All cryptographic operations perform locally.

---

*Integration audit: 2026-05-01*