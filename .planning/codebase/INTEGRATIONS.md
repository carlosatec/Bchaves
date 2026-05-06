---
last_mapped_date: 2026-05-02
---
# Integrations

## External Dependencies
**None.** The project is entirely self-contained bare-metal C++ code. It deliberately avoids any external libraries (such as OpenSSL, libgmp, or libsecp256k1) to maintain absolute control over the execution path, memory layout, and SIMD dispatching.

## System Interfaces
- **Filesystem**: Reads target addresses/public keys from plain text files and writes checkpoint states to disk to prevent progress loss.
- **Hardware/OS Detection**: Directly probes CPU flags (via `cpuid` or ARM equivalent), thread counts, NUMA topology, and cache sizes to dynamically "Auto-Tune" batch processing sizes and thread assignments.

## Network Interfaces
**None.** The tool is entirely offline. No APIs or external webhooks are called, preserving total privacy of the search process.