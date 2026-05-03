# Project Structure

## Directory Overview

### `/core`
Contém as primitivas criptográficas e aritmética de campo.
- `secp256k1.cpp/hpp`: Implementação escalar de referência.
- `secp256k1-*.hpp`: Versões otimizadas via SIMD (SSE, AVX, ARM64).
- `secp256k1_reduce.hpp`: Helpers de redução modular rápida.
- `hash.cpp/hpp`: Wrapper de funções de hash.
- `sha256-*.cpp`: Implementações SIMD de SHA256.
- `ripemd160-*.cpp`: Implementações SIMD de RIPEMD160.
- `adaptive_filter.cpp/hpp`: Despachante de filtragem multi-alvo.
- `adaptive_filter_*.cpp`: Kernels SIMD (AVX512, AVX2, SSE4, NEON).

### `/engine`
Implementação dos algoritmos de busca.
- `address.cpp`: Motor de busca por endereços (Hybrid mode).
- `bsgs.cpp`: Motor Baby-step Giant-step para busca em intervalos.
- `kangaroo.cpp`: Motor Pollard's Kangaroo para busca em intervalos grandes.
- `app.cpp`: Orquestrador de threads e ciclo de vida do motor.

### `/system`
Abstrações de hardware e sistema operacional.
- `hardware.cpp`: Detecção de CPU, topologia e capacidades SIMD.
- `cli.cpp`: Parsing de argumentos de linha de comando.
- `checkpoint.cpp`: Persistência de estado e recuperação de sessão.
- `io.cpp`: Gerenciamento de arquivos de traps e logs.
- `types.hpp`: Definição de tipos globais e constantes.

### `/modulos`
Pontos de entrada (main) para os diferentes alvos de binário.
- `address.cpp`, `bsgs.cpp`, `kangaroo.cpp`.

### `/tests`
Suítes de validação.
- `crypto_test.cpp`: Testes de corretude escalar.
- `simd_test.cpp`: Validação de kernels SIMD contra referência.
- `test_cuckoo_adaptive.cpp`: Validação do filtro probabilístico SIMD.
- `test_crypto_arm64.cpp`: Validação de kernels de hardware ARMv8.