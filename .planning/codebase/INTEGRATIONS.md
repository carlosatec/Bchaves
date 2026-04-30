# Integrations

## System Integrations
- **CLI (Command Line Interface)**: Primary interface for user interaction and configuration.
- **Filesystem**: 
    - **Checkpointing**: State persistence for long-running search jobs.
    - **Target Files**: Loading list of public keys/addresses to search for.
    - **Output Logging**: Saving found keys and performance metrics.

## Hardware Integrations
- **CPU Instruction Sets**: 
    - **AVX2 / AVX-512**: Used for vectorized arithmetic (where available).
    - **ARM Crypto Extensions**: Used for accelerated hashing on ARM64.

## Network Integrations
- **None**: The engine operates entirely offline for security and speed.
