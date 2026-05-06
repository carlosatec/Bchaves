# Integrations

Last mapped: 2026-05-06

## External Services

**None.** Bchaves is a fully offline, self-contained computation engine. It does not connect to any external API, database, or network service.

## File System I/O

| Purpose | Format | Location |
|---|---|---|
| Target addresses | Plain text file (one per line) | User-specified via `--target` |
| Checkpoints | Binary with CRC32 header | `.ckp` files in working directory or `--checkpoint` path |
| Kangaroo traps | Binary shard files | `traps/` directory or `--trap-dir` path |
| Puzzle files | Text/hex | `puzzles/` directory |

## OS-Level Integrations

| Feature | Linux | Windows (WSL) |
|---|---|---|
| Thread affinity | `pthread_setaffinity_np` | `SetThreadAffinityMask` / `SetThreadGroupAffinity` |
| NUMA detection | `/proc/cpuinfo`, sysfs | `GetLogicalProcessorInformationEx` |
| Signal handling | `signal(SIGINT, ...)` | Same via WSL |
| CPU feature detection | `__get_cpuid_count` (x86), `/proc/cpuinfo` (ARM) | Same via WSL |

## Hardware Dependencies

- x86-64 CPU with SSE4.1+ for SIMD acceleration (falls back to scalar)
- ARM64 with NEON + optional Crypto Extensions for ARM builds
- Minimum ~4 GB RAM recommended for Kangaroo trap tables