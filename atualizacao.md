# [PLAN] Plano de Atualização: Motor Quântico-Inspirado (SIMD-AVX512)

Este documento detalha as etapas para transformar o motor **Bchaves** em um cracker de alta performance utilizando instruções vetoriais e heurísticas de rejeição antecipada.

---

## 🟢 Fase 1: Infraestrutura de Hardware e Branch Experimental
**Objetivo:** Preparar o ambiente para suportar instruções de 512 bits e garantir alinhamento de memória.

1.  **Criação do Workspace:**
    *   Duplicar `engine/address.cpp` para `engine/address_simd.cpp`.
    *   Criar `core/simd_math.hpp` para abstração de intrínsecos (`immintrin.h`).
2.  **Configuração do Compilador (Makefile):**
    *   Adicionar flags de otimização agressiva: `-O3 -march=native -mavx512f -mavx512dq -mavx512vl -funroll-loops`.
    *   Implementar verificação em tempo de compilação para suporte a AVX-512.
3.  **Alinhamento de Memória (64-byte boundary):**
    *   Substituir alocações de buffers de chaves por `_mm_malloc(size, 64)` em `address_simd.cpp`.
    *   Garantir que as structs de Pontos Jacobian e Affine estejam alinhadas para evitar `GPF` (General Protection Fault) em instruções vetoriais.

## 🟡 Fase 2: Novo Modo de Busca "Quantum Walk" (Salto de Shor)
**Objetivo:** Criar um novo modo de operação (`-m quantum`) que substitui a busca linear exaustiva por uma progressão geométrica probabilística, focando em máxima difusão pelo espaço (ideal para cenários onde não se deseja varrer ranges sequenciais).

1.  **Geração da Tabela de Saltos (Respeitando o Puzzle):**
    *   Implementar `generate_de_bruijn_table()` em `core/math.cpp`.
    *   *Limite de Escala:* Os valores dos saltos devem ser matematicamente calibrados com base no tamanho do Puzzle (ex: para o Puzzle 71, o range é $2^{70}$). Os saltos médios devem ser proporcionais a uma fração segura do range para garantir que a difusão ocorra *dentro* do labirinto sem "pular o muro" logo no início.
    *   Para garantir alta performance, os pontos correspondentes a esses saltos (`Jump_P = salto * G`) devem ser pré-calculados.
2.  **Integração em um Novo Worker (Busca Probabilística):**
    *   Criar a função `run_quantum_worker` no `address.cpp`.
    *   **Boundary Check (Efeito Bumerangue):** O worker precisará de uma checagem leve. Se `key + salto > range_end`, em vez de somar, a thread subtrai o salto (`tmp_jac = add_points(tmp_jac, neg_Jump_P)`), "quicando" na parede do puzzle e voltando para dentro do range.
    *   **A Lógica de Incremento:** `tmp_jac = add_points(tmp_jac, Jump_P[hash16(last_bits)])` (ou `neg_Jump_P` se quicar).
    *   *Nota de Checkpoint:* O checkpoint armazenará o estado atual da semente, chave atual e direção de cada thread, permitindo retomar as caminhadas exatas de onde pararam.

---

## 🔴 Fase 3: Vetorização ECC e Oráculo de Grover (O Coração do Plano)
**Objetivo:** Processar 16 chaves simultaneamente e rejeitá-las o mais rápido possível.

1.  **SIMD Point Addition (AVX-512):**
    *   Implementar a matemática de campo (soma, subtração, multiplicação modular) operando sobre 16 registros de 64 bits em paralelo.
    *   Criar `secp256k1_add_16x()`, calculando 16 novos pontos da curva em um único ciclo de execução vetorial.
2.  **Transposição de Registradores:**
    *   Lógica para converter 16 coordenadas X de chaves públicas em um formato de coluna para o SHA-256.
3.  **Oráculo de Rejeição (Grover-SIMD Masking):**
    *   *Correção Criptográfica:* Como o Puzzle 71 é um P2PKH (só temos o Hash160, não a chave pública), é matematicamente impossível parar no round 16 do SHA-256, pois a inversão do RIPEMD-160 é inviável.
    *   **A verdadeira implementação do Oráculo:** Executaremos o SHA-256 e o RIPEMD-160 completos, porém de forma **100% vetorizada (AVX-512)** para os 16 blocos.
    *   **Heurística de Colapso (Anulação de Amplitude):** No final do RIPEMD-160 vetorial, em vez de extrair os 16 hashes para a memória RAM (o que causa gargalo), aplicamos uma instrução `_mm512_cmpeq_epi32_mask` comparando o vetor resultante com um registrador estático contendo o Hash160 do Puzzle 71.
    *   Se a máscara for `0` (nenhum match), a CPU descarta os 16 resultados em 1 ciclo de clock, "poupando" a extração de memória e validações escalares (como buscas binárias em arrays de alvos).

---

## 🔵 Fase 4: Telemetria e Monitoramento de Estresse
**Objetivo:** Garantir que o aumento de performance não cause instabilidade térmica.

1.  **Atômicos de Alta Velocidade:**
    *   Mudar todos os contadores globais para `std::atomic<uint64_t>` com `std::memory_order_relaxed`.
2.  **Exportador de Métricas:**
    *   Implementar escrita silenciosa de `M/s` em `/dev/shm/bchaves.stats` (ou arquivo equivalente em Windows como `C:\Temp\bchaves.stats`).
3.  **Dashboard Externo (Zabbix/Grafana):**
    *   Script auxiliar para ler a telemetria e plotar a curva de hash vs temperatura da CPU.

---

## 🛡️ Protocolo de Validação
Cada fase só será integrada após passar pelos seguintes testes:
1.  **Teste de Integridade:** As chaves geradas pelo motor SIMD devem ser idênticas às do motor escalar original.
2.  **Teste de Colisão:** Confirmar que o "Salto de Shor" não gera chaves repetidas em diferentes threads.
3.  **Benchmark de Oráculo:** Validar se o ganho de performance da filtragem via máscara SIMD (evitando extração para RAM e IFs escalares) compensa o overhead da transposição de registros.

---
*Documento gerado como base para a implementação do Motor Quântico-Inspirado.*
