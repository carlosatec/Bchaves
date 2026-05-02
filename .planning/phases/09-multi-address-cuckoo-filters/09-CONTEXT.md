# Phase 09: Multi-Address Cuckoo Filters - CONTEXT

## Domain
Implementation of a high-capacity, hardware-adaptive Cuckoo Filter for simultaneous search of millions of Bitcoin addresses (HASH160) integrated into the vectorized Kangaroo and Address engines.

## Locked Decisions
- **Filter Type**: Cuckoo Filter (4-entry buckets, 16-bit fingerprints).
- **Hardware Adaptability**:
    - **SIMD Dispatch**: Dynamic selection of static kernels (AVX512, AVX2, SSE4.1, NEON) via function pointers initialized at startup based on `HardwareInfo`.
    - **Cache Alignment**: Bucket layout and sharding factors dynamically adjusted to fit CPU Cache Line (64 bytes).
- **Hashing Strategy**: "Zero-Cost Hashing" utilizing bits from the source HASH160 for fingerprints and primary indices. Secondary index derivation using hardware-accelerated CRC32 (`_mm_crc32_u64`).
- **Scale & Memory**:
    - **NUMA Awareness**: Support for local-node filter replication or partitioning to prevent cross-socket memory latency.
    - **Huge Pages**: Utilization of HugeTLB (Linux) or Large Pages (Windows) for large filters (100M+ targets) to minimize TLB misses.
- **Search Integration**: Vectorized lookup (SoA) checking 8-16 fingerprints in parallel per SIMD instruction.

## Canonical Refs
- `system/hardware.cpp`: Source of `HardwareInfo` for adaptation.
- `engine/address.cpp`: Main integration point for address search.
- `core/secp256k1_fleet.hpp`: Reference for SoA fleet layout.

## Code Context
- Reusable bucket displacement logic from existing trap table.
- SIMD dispatcher pattern from Phase 08.
