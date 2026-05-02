---
phase: 06-hardware-auto
plan: 01
type: execute
wave: 1
depends_on: []
files_modified: []
autonomous: true
requirements:
  - HW-01
  - HW-02
  - HW-03
  - AUTO-01
  - AUTO-02
  - AUTO-03

must_haves:
  truths:
    - "Hardware detection correctly identifies CPU features (AVX2, BMI2, SHA-NI, AVX-512) on x86_64"
    - "Hardware detection correctly identifies NEON on ARM64"
    - "Auto-tune calculates optimal thread count based on physical cores"
    - "Auto-tune calculates optimal batch size based on L3 cache size"
    - "Auto-tune calculates optimal table_k based on available RAM"
  artifacts:
    - path: "system/hardware.hpp"
      provides: "Hardware detection with enhanced CPU brand string detection"
      min_lines: 250
    - path: "system/hardware.hpp"
      provides: "Auto-tune with hardware-adaptive profiles"
      min_lines: 300
  key_links:
    - from: "system/hardware.cpp"
      to: "system/types.hpp"
      via: "TuneProfile, HardwareInfo struct definitions"
      pattern: "struct TuneProfile|struct HardwareInfo"
    - from: "engine/address.cpp"
      to: "system/hardware.cpp"
      via: "tune_for() call"
      pattern: "tune_for.*hardware"
    - from: "engine/bsgs.cpp"
      to: "system/hardware.cpp"
      via: "tune_for() call"
      pattern: "tune_for.*hardware"
    - from: "engine/kangaroo.cpp"
      to: "system/hardware.cpp"
      via: "tune_for() call"
      pattern: "tune_for.*hardware"
---

<objective>
Improve hardware detection and auto mode for the Bitcoin performance engine.

**Purpose:** Enhance hardware detection to correctly identify CPU features, cache sizes, and system memory. Improve auto-tune to calculate optimal thread counts, batch sizes, and table sizes based on detected hardware.

**Output:** Enhanced `system/hardware.ipp` with improved detection and auto-tuning logic.
</objective>

<execution_context>
@C:/Users/Carlos/.config/opencode/get-shit-done/workflows/execute-plan.md
@C:/Users/Carlos/.config/opencode/get-shit- done/templates/summary.md
</execution_context>

<context>
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/codebase/ARCHITECTURE.md
@.planning/codebase/CONCERNS.md
@system/types.hpp
@system/hardware.cpp
@system/hardware.hpp
</context>

<introduction>
## Gap Analysis: Hardware Detection and Auto Mode

### Current Implementation Analysis

**Strengths:**
1. Basic CPU feature detection via CPUID (AVX2, BMI2, SHA-NI, SSE4)
2. RAM detection via platform APIs (Windows/GlobalMemoryStatusEx, Linux/sysinfo)
3. L3 cache detection via CPUID Leaf 4 (x86_64 only)
4. Simple auto-tune profiles (safe/balanced/max)
5. Physical core estimation via logical/2 on x86_64

**Weaknesses (Gaps):**
1. **No CPU brand string extraction** - Cannot distinguish Xeon vs Core i7 vs Ryzen
2. **No L1/L2 cache detection** - Hardcoded values (32KB/256KB)
3. **No NUMA detection** - `is_numa` always false on Windows
4. **No topology detection** - No CCX/NUMA node awareness for thread affinity
5. **No memory bandwidth estimation** - Cannot adapt batch size to memory speed
6. **No power profile detection** - Cannot detect thermal throttling state
7. **Hardcoded L3 fallback** - 8MB heuristic when CPUID fails
8. **No AVX-512 sub-detection** - Detects AVX-512F but not VL/BW/DQ
9. **Inconsistent physical core calculation** - Assumes HyperThreading, breaks on AMD SMT
10. **Auto-tune ignores L1/L2 cache** - Batch sizes not tuned to L1d/L1i
11. **Auto-tune ignores CPU brand** - Same settings for Xeon and Core i7
12. **Thread count ignores NUMA** - No awareness of inter-socket latency

### Requirements Addressed

| ID | Requirement | Current State | Gap |
|----|-------------|--------------|-----|
| HW-01 | Detect all CPU features | Partial | Missing: CPU brand, topology, memory bandwidth |
| HW-02 | Detect cache hierarchy | Hardcoded | Missing: L1/L2 detection, per-core caches |
| HW-03 | Detect memory system | Basic RAM | Missing: NUMA, bandwidth, ECC |
| AUTO-01 | Auto-tune threads | Physical/2 | Wrong: Assumes HyperThreading |
| AUTO-02 | Auto-tune batch | Simple mult | Wrong: Ignores cache hierarchy |
| AUTO-03 | Auto-tune table_k | RAM-based | Partial: Missing memory bandwidth |
</introduction>

<tasks>

<task type="auto" tdd="true">
  <name>Enhance hardware detection with CPU brand string and topology</name>
  <files>system/hardware.cpp</files>
  <behavior>
    - Test 1: detect_hardware() returns non-empty CPU brand string on x86_64
    - Test 2: detect_hardware() returns num_cores >= 1
    - Test 3: detect_hardware() returns num_physical_cores >= 1
    - Test 4: detect_hardware() returns L1/L2/L3 cache sizes > 0
    - Test 5: detect_hardware().is_numa reflects actual NUMA topology
    - Test 6: detect_hardware() returns correct CPU features (AVX2, BMI2, SHA-NI, AVX-512)
  </behavior>
  <action>
    **Improve detect_hardware() with enhanced detection:**

    1. **CPU Brand String Extraction:**
       - Windows: Read registry `HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\SYSTEM\CentralProcessor\0`
       - Linux: Read `/proc/cpuinfo` line "model name"
       - ARM64: Read `/proc/cpuinfo` line "Model"

    2. **L1/L2 Cache Detection:**
       - Use CPUID Leaf 4 with ECX=0 for L1 data cache
       - Use CPUID Leaf 4 with ECX=1 for L2 cache (if shared)
       - Add detection for L1 instruction cache
       - Add detection for per-core vs shared L2

    3. **Improved Physical Core Detection:**
       - Detect AMD Ryzen SMT (AMD uses different APIC IDs)
       - Detect Intel Hyper-Threading pattern
       - Use Linux `/sys/devices/system/cpu/cpu*/topology/core_id` for accurate count
       - Windows: Use `GetLogicalProcessorInformationEx()`

    4. **NUMA Detection:**
       - Windows: Use `GetNumaHighestNodeNumber()` and `GetNumaNodeProcessorMask()`
       - Linux: Check `/sys/devices/system/node/` for multiple nodes
       - Set `is_numa = true` if >1 NUMA node detected

    5. **CPU Topology Detection:**
       - Detect CCX boundaries on AMD Ryzen (L3 cache topology)
       - Detect cache topology (which cores share L2/L3)
       - Detect memory channels for memory bandwidth estimation

    6. **Improved Feature Detection:**
       - Detect AVX-512 sub-features: AVX512VL, AVX512BW, AVX512DQ, AVX512ER
       - Add detection for MWAIT (monitor/mwait for low-power)
       - Add detection for SSE4.2 (PCMPGTQ, CRC32 instructions)
       - Detect VAES (vector AES for AES-NI via AVX)
       - Detect GFNI (Galois Field instructions)

    7. **Memory Bandwidth Estimation:**
       - Estimate based on channel count (1-4 channels typical)
       - Detect DDR generation (DDR4 vs DDR5 vs LPDDR)
       - Estimate peak bandwidth: channels * gen_bandwidth * width

    **Struct Updates:**
    ```cpp
    struct HardwareInfo {
        // Existing fields...
        std::string cpu_brand;           // NEW: "Intel Core i7-9700K"
        std::uint32_t num_numa_nodes;      // NEW: NUMA node count
        std::uint32_t memory_channels;      // NEW: DRAM channels
        std::uint32_t memory_gen;        // NEW: DDR4=4, DDR5=5
        std::uint64_t estimated_bandwidth; // NEW: MB/s peak
        bool is_smt_enabled;            // NEW: HyperThreading/SMT
        std::uint32_t cacheTopology[4]; // NEW: cores per L1/L2/L3
    };
    ```

    **Error Handling:**
    - Return sensible defaults if any detection fails
    - Log detection failures at trace level
    - Never crash on detection failure
  </action>
  <verify>
    <automated>build/crypto_test 2>&1 | grep -E "(Hardware|CPU|RAM|Cache)" | head -20</automated>
  </verify>
  <done>
    - CPU brand string extracted and displayed
    - L1/L2/L3 cache sizes correctly detected
    - Physical core count accurate on Intel and AMD
    - NUMA topology detected when present
    - All CPU features correctly detected (AVX2, BMI2, SHA-NI, AVX-512)
  </done>
</task>

<task type="auto" tdd="true">
  <name>Improve auto-tune with hardware-adaptive profiles</name>
  <files>system/hardware.cpp</files>
  <behavior>
    - Test 1: tune_for(hw, safe) returns threads <= num_physical_cores
    - Test 2: tune_for(hw, balanced) returns threads == num_physical_cores
    - Test 3: tune_for(hw, max) returns threads == num_cores (with SMT)
    - Test 4: batch_size calculated based on L3 cache (not just AVX2 flag)
    - Test 5: table_k scales with available RAM, not total RAM
    - Test 6: tune_for() returns sensible values on unknown CPU
  </behavior>
  <action>
    **Improve tune_for() with hardware-adaptive profiles:**

    1. **Thread Count Calculation:**
       ```
       if (profile == safe):
           // Use half of physical cores, account for SMT if enabled
           if (hw.is_smt_enabled):
               threads = hw.num_physical_cores / 2
           else:
               threads = hw.num_physical_cores / 2
           threads = max(1, threads)

       if (profile == balanced):
           threads = hw.num_physical_cores  // No SMT benefits for crypto

       if (profile == max):
           threads = hw.num_cores  // SMT helps with memory latency hiding
       ```

    2. **Batch Size Calculation Based on Cache Hierarchy:**
       ```
       // Optimal batch size: L1D can hold ~32KB, aim for 2-4x L1D
       // But also bounded by L3: each batch entry ~32-64 bytes
       // L3-constrained: batch_size * entry_size <= L3_cache * 0.5
       base = hw.l3_cache / 64  // entries that fit in L3 half
       base = min(base, 32 * 1024)  // cap at 32K entries

       if (hw.features & cpu_avx2):
           base *= 4  // 4x throughput with SIMD
       if (hw.l3_cache >= 16MB):
           base *= 2  // Large L3 allows bigger batches
       ```

    3. **Table Size Calculation Based on Memory Budget:**
       ```
       available = hw.ram_available * 7 / 10  // 70% budget
       if (hw.num_numa_nodes > 1):
           available /= hw.num_numa_nodes  // Per-node budget

       // Account for memory bandwidth
       if (hw.estimated_bandwidth < 30GB/s):
           available *= 3 / 4  // Reduce for slow memory

       table_k = available / entry_size  // entry ~48 bytes
       table_k = clamp(table_k, 512, 8MB)
       ```

    4. **CPU-Specific Tuning Hints:**
       ```
       // Detect CPU family for specific tuning
       if (cpu_brand contains "Xeon"):
           // Xeon: Prefer fewer threads, larger batches
           batch *= 2
           threads = min(threads, hw.num_physical_cores)

       if (cpu_brand contains "Core i7" or "Ryzen"):
           // Desktop/Consumer: More aggressive
           batch *= 1.5

       if (cpu_brand contains "i9" or "Threadripper"):
           // HEDT: Full SMT benefit
           batch *= 2
           threads = hw.num_cores
       ```

    5. **NUMA-Aware Thread Distribution:**
       ```
       if (hw.is_numa):
           // Distribute threads evenly across nodes
           threads_per_node = threads / hw.num_numa_nodes
           // Set thread affinity hint
           pin_threads_to_node(hw.num_numa_nodes)
       ```

    6. **Memory Pressure Detection:**
       ```
       // Estimate memory pressure at startup
       pressure = hw.ram_available / hw.ram_total
       if (pressure < 0.3):
           // Less than 30% free - reduce table_k significantly
           table_k *= pressure / 0.3
       ```

    **Error Handling:**
    - Return minimum values (1 thread, 256 batch, 512 table_k) on detection failure
    - Never return 0 for any tune parameter
  </action>
  <verify>
    <automated>build/crypto_test --benchmark 2>&1 | grep -E "(Perfil|Threads|Batch|Table)" | head -10</automated>
  </verify>
  <done>
    - Thread count adapts to SMT status
    - Batch size adapts to L3 cache size
    - Table size adapts to available RAM (not total)
    - CPU brand affects tuning decisions
    - NUMA-aware thread distribution works
  </done>
</task>

<task type="auto">
  <name>Add --list-hardware CLI flag for hardware report</name>
  <files>system/cli.ipp, modulos/address.ipp, modulos/bsgs.ipp, modulos/kangaroo.ipp</files>
  <action>
    **Add comprehensive hardware report via --list-hardware flag:**

    1. **Display Format:**
       ```
       $ ./address targets.txt -b 32 --list-hardware
       [*] Hardware Report:
           CPU: Intel Core i7-9700K @ 3.60GHz
           Cores: 8 (Physical: 8, Logical: 8, SMT: No)
           Cache: L1d=32KB, L2=256KB, L3=12MB
           Memory: 32GB DDR4 (Available: 28GB, Channels: 2)
           Features: AVX2, BMI2, SHA-NI, SSE4.2
           NUMA: No (1 node)
           Estimated Bandwidth: 38.4 GB/s
           Recommended Tuning:
             Threads: 8, Batch: 1024, Table: 589824
       ```

    2. **Implementation:**
       - Add `--list-hardware` flag parsing in CLI
       - When flag present, print report and exit (don't run search)
       - Include all detected fields in report
       - Include recommended tune values

    3. **Exit Code:**
       - Return 0 on success (--list-hardware is informational)
       - Return 1 only on detection failure (which shouldn't happen)

    **Integration Points:**
    - Add to `parse_common_flag()` in cli.ipp
    - Add help text in `*_help()` functions
    - Update all engines (address, bsgs, kangaroo) to use common flag
  </action>
  <verify>
    <automated>./build/address targets.txt -b 32 --list-hardware 2>&1 | grep -E "Hardware|CPU|Cores|Cache|Memory|Features"</automated>
  </verify>
  <done>
    - --list-hardware displays full hardware report
    - Report includes all detected fields
    - Report includes recommended tuning
    - Works on all engines (address, bsgs, kangaroo)
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Hardware detection | No untrusted input crosses this boundary - detection uses privileged CPUID/Syscall |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|----------------|
| T-06-01 | Denial of Service | Hardware detection crashes on unknown CPU | mitigate | Return defaults on detection failure, never crash |
| T-06-02 | Information Disclosure | CPU brand string may expose environment | accept | Brand string is non-sensitive system information |
| T-06-03 | Denial of Service | Auto-tune returns 0 threads on failure | mitigate | Always clamp to minimum 1 thread |
</threat_model>

<verification>
**Phase Verification:**

1. **Hardware Detection Tests:**
   - `./build/crypto_test --benchmark` runs without crash
   - `--list-hardware` outputs all fields correctly
   - CPU brand string is non-empty
   - Cache sizes are > 0

2. **Auto-tune Tests:**
   - Thread count is >= 1 for any profile
   - Batch size is > 0 for any profile
   - Table size is > 0 for any profile

3. **Integration Tests:**
   - All engines use enhanced detection
   - No duplicate `detect_hardware()` calls per engine
   - `--list-hardware` works on all engines
</verification>

<success_criteria>
1. Hardware detection correctly identifies all CPU features on test system
2. Auto-tune returns thread/batch/table values within expected ranges
3. `--list-hardware` flag displays complete hardware report
4. No regressions in existing engine functionality
5. Code compiles cleanly with -Wall -Wextra
</success_criteria>

<output>
After completion, create `.planning/intel/06-hardware-auto/06-01-SUMMARY.ipp`
</output>