# Bchaves

Base inicial da reescrita do projeto com foco em compatibilidade Linux/Windows.

## O que esta base entrega
- Build portátil com CMake e presets.
- Binários `address`, `bsgs` e `kangaroo`.
- Parsing e validação de alvos Bitcoin base58/public key.
- Leitura dos arquivos reais em `Puzzles/` e suporte a alvo inline quando aplicável.
- Backend portátil de referência para secp256k1, sem dependência externa obrigatória.
- Backends de referência funcionais para `address`, `bsgs` e `kangaroo`.
- Checkpoint binário portátil com CRC32, auto-save, resume e save de emergência em `Ctrl+C`.
- Auto-tuning inicial e detecção básica de hardware.
- Saída padronizada para progresso e métricas.

## O que ainda falta
- Backend otimizado com `libsecp256k1` e GLV/wNAF.
- Hot path otimizado com batching e multithreading real.
- Implementação algorítmica otimizada de verdade para BSGS e Kangaroo.

## Backend secp256k1
Por padrão, a base usa o backend portátil interno.

Também é possível compilar com `libsecp256k1` como backend opcional:

Com CMake:
```bash
cmake --preset default -DQCHAVES_ENABLE_LIBSECP256K1=ON
cmake --build --preset default
```

Com os scripts WSL:
```bash
QCHAVES_ENABLE_LIBSECP256K1=1 bash scripts/build_wsl.sh
QCHAVES_ENABLE_LIBSECP256K1=1 bash scripts/test_wsl.sh
```

Se o header/lib não estiverem em paths padrão, use:
```bash
export LIBSECP256K1_INCLUDE_DIR=/caminho/include
export LIBSECP256K1_LIBRARY=/caminho/libsecp256k1.so
QCHAVES_ENABLE_LIBSECP256K1=1 bash scripts/build_wsl.sh
```

## Build
```bash
cmake --preset default
cmake --build --preset default
```

Para flags agressivas específicas da máquina:
```bash
cmake --preset native
cmake --build --preset native
```

Se o WSL não tiver `cmake`, há scripts de build/teste com `g++`:
```bash
bash scripts/build_wsl.sh
bash scripts/test_wsl.sh
```

## Pre-requisitos por ambiente

### Linux
Recomendado para rodar e testar com menos atrito:
- `g++` ou `clang++` com suporte a C++17
- `cmake >= 3.20`
- `make` ou `ninja`

Exemplo em Debian/Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
```

Build padrão:
```bash
cmake --preset default
cmake --build --preset default
ctest --test-dir build/default --output-on-failure
```

Se quiser testar sem `cmake`, o caminho mínimo é:
```bash
bash scripts/build_wsl.sh
bash scripts/test_wsl.sh
```

### WSL
O projeto roda bem em `WSL2` com Ubuntu/Debian, desde que o toolchain Linux esteja instalado dentro da distro.

Pacotes recomendados no WSL:
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build gdb
```

Exemplo de uso dentro do WSL:
```bash
cd /mnt/d/Workspace/Bchaves
cmake --preset default
cmake --build --preset default
ctest --test-dir build/default --output-on-failure
```

Fallback quando `cmake` ainda não estiver instalado:
```bash
cd /mnt/d/Workspace/Bchaves
bash scripts/build_wsl.sh
bash scripts/test_wsl.sh
```

### Windows
Para build nativo no Windows, use um compilador C++17 e `cmake`.

Opções recomendadas:
- Visual Studio 2022 ou Build Tools 2022 com workload `Desktop development with C++`
- `CMake >= 3.20`

Exemplo com Developer PowerShell:
```powershell
cmake --preset default
cmake --build --preset default
ctest --test-dir build/default --output-on-failure
```

Observações para Windows:
- `QCHAVES_ENABLE_NATIVE=ON` ativa flags agressivas e deve ser usado só na máquina onde o binário será executado.
- Se o ambiente Windows não tiver toolchain C++ configurado, o caminho mais simples para testar localmente costuma ser o `WSL2`.

## Uso
```bash
./address Puzzles/1.txt -R sequential -l both
./address Puzzles/71.txt -R sequential -l compress --start 0x400000000000000000 --end 0x7fffffffffffffffff --limit 100000
./bsgs Puzzles/1.txt -b 20 -k 2048
./kangaroo Puzzles/1.txt -r 0:FFFFFFFF
./kangaroo 02145d2611c823a396ef6712ce0f712f09b9b4f3135e3e0aa3230fb9b6d08d1e16 -r 0:FFFFFFFF
```

Notas do modo `address`:
- Se o alvo estiver em `Puzzles/<n>.txt`, o range padrão é inferido do número do puzzle.
- Também é possível definir o range explicitamente com `--start` e `--end`.
- `--limit` ajuda a testar ranges grandes com o backend portátil atual.

Notas dos modos `bsgs` e `kangaroo`:
- Nesta base, ambos usam backend portátil de referência para busca funcional e integração.
- O fluxo de `checkpoint`, `resume` via `QCHVES_RESUME=1` e `Ctrl+C` já funciona nesses modos.
- Eles ainda não usam as otimizações esperadas no plano original, então servem como base correta e portável, não como implementação final de performance.

## Observações de portabilidade
- `-march=native` fica desabilitado por padrão para preservar portabilidade binária.
- Afinidade de CPU e métricas avançadas de cache devem ser implementadas por camada de plataforma, não com `pthread` direto no código alto nível.
