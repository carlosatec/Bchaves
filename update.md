# Plano de Revisao e Performance - Bchaves

> Escopo inicial desta analise: leitura completa da estrutura principal (`core/`, `engine/`, `system/`, `modulos/`, `Makefile`, `README`, docs) sem alterar o codigo.  
> Status de execucao em `2026-04-29`: as correcoes marcadas no checklist abaixo ja foram aplicadas no fonte, mas ainda nao foi possivel compilar ou rodar benchmark localmente porque o shell atual nao possui `make` nem `g++`. A validacao restante continua estatica.

## Objetivo

Corrigir erros funcionais e estruturar as melhorias de velocidade sem tocar no codigo agora. A prioridade foi:

1. preservar corretude criptografica
2. evitar travamentos e cobertura incompleta
3. aumentar throughput real
4. melhorar robustez de build, CLI e checkpoints

## Resumo Executivo

Os problemas mais graves hoje nao estao nas micro-otimizacoes; estao em corretude e controle de execucao:

- `BSGS` e `Kangaroo` aceitam pubkeys comprimidas, mas o codigo nao reconstrui `Y`, o que invalida o ponto de entrada para esses motores.
- `BSGS` e `Kangaroo` usam apenas o `x.limbs[0]` como identidade do ponto em etapas criticas; isso abre falso positivo e chave errada.
- o `address` pode ficar preso para sempre quando os workers terminam sem encontrar nada.
- `batch_normalize()` suporta no maximo `4096` pontos em stack, mas `tune_for(max)` pode entregar `8192`, o que abre espaco para overflow de buffer/stack corruption.
- o WIF derivado hoje esta incorreto porque falta `version byte` e checksum Base58Check.

## Prioridade 0 - Corrigir antes de qualquer tuning

### P0.1 - Pubkey comprimida desserializada de forma incompleta
- Arquivos: `core/secp256k1.cpp:652-669`, `engine/bsgs.cpp:70`, `engine/kangaroo.cpp:329`
- Problema: ao ler pubkey `02/03...`, `deserialize_pubkey()` popula apenas `x` e retorna sem reconstruir `y`.
- Impacto: `BSGS` e `Kangaroo` passam a operar sobre ponto invalido para entradas comprimidas.
- Plano:
  - implementar recuperacao de `y` pela equacao da curva `y^2 = x^3 + 7 mod p`
  - escolher a raiz correta usando o prefixo `02`/`03`
  - validar se o ponto reconstruido pertence a `secp256k1`
  - rejeitar pubkeys invalidas explicitamente
- Validacao:
  - teste unitario com pubkey comprimida conhecida
  - comparar a pubkey descomprimida gerada com vetor de teste conhecido
  - repetir em `BSGS` e `Kangaroo`

### P0.2 - Identidade de ponto reduzida a 64 bits em BSGS e Kangaroo
- Arquivos: `engine/bsgs.cpp:99-102`, `engine/bsgs.cpp:163-171`, `engine/kangaroo.cpp:428-470`
- Problema: o codigo usa `x.limbs[0]` como hash/identidade do ponto e, em `BSGS`, aceita match apenas por esse valor. Em `Kangaroo`, a trap tambem e indexada so por esse `uint64_t`.
- Impacto:
  - colisao por truncamento de 256 para 64 bits
  - ambiguidade entre `P` e `-P`, que compartilham o mesmo `x`
  - risco real de solucao falsa ou chave errada
- Nota de prioridade:
  - a preocupacao com colisao aleatoria de 64 bits isoladamente seria baixa em ranges pequenos
  - o problema continua critico porque `BSGS` multithread pode aceitar match incorreto por `x` ao encontrar `-P` antes do worker que encontraria `P` correto
  - em `Kangaroo`, usar apenas `x` tambem nao distingue `P` de `-P`, o que e estruturalmente inseguro para detectar colisao de grupo
- Plano:
  - elevar a identidade minima para `x completo + paridade de y`
  - em `BSGS`, armazenar e validar o ponto completo ou `x + odd(y)` antes de aceitar a solucao
  - em `Kangaroo`, salvar trap com chave forte, no minimo `x completo + parity`
  - verificar a solucao final recomputando a pubkey e comparando com o alvo antes de reportar
- Validacao:
  - testes com pares `P` e `-P`
  - testes artificiais de colisao em hash curto
  - validacao final obrigatoria da chave encontrada contra a pubkey alvo

### P0.4 - Loop principal do `address` nao detecta fim real dos workers
- Arquivo: `engine/address.cpp:625-674`
- Problema: o codigo tenta detectar conclusao checando `joinable()`, mas `std::thread::joinable()` continua `true` ate o `join`. Com isso, `all_done` nunca vira `true`.
- Impacto: se a busca terminar sem encontrar chave e sem `SIGINT`, o processo pode ficar preso no loop de monitoramento indefinidamente.
- Plano:
  - substituir a heuristica por contador atomico de workers ativos ou `std::latch`
  - sinalizar fim de cada worker explicitamente
  - encerrar o loop principal quando `active_workers == 0`
- Validacao:
  - range pequeno sem target
  - confirmar encerramento limpo em `sequential`, `backward`, `both` e `hybrid`

### P0.5 - Overflow potencial em `batch_normalize()`
- Arquivos: `core/secp256k1.cpp:513-522`, `system/hardware.cpp:158-170`, `engine/address.cpp:516`, `engine/address.cpp:540-542`
- Problema: `batch_normalize()` reserva stack fixa para `4096` entradas, mas `tune_for(max)` pode produzir `batch_size = 8192`.
- Impacto: escrita fora do buffer, corrupcao de stack e comportamento indefinido em execucao pesada.
- Plano:
  - colocar `guard clause` em `batch_normalize` e falhar rapido se `count > MAX_STACK`
  - ou migrar `prod_storage` para buffer dinamico reutilizavel por thread
  - alinhar `tune_for()` aos limites reais do core matematico
- Validacao:
  - teste com `-A max`
  - sanitizers/ASan quando houver toolchain
  - smoke test de longa duracao

### P0.6 - WIF esta sendo gerado sem versao e checksum
- Arquivo: `core/address.cpp:29-45`, `core/address.cpp:101-105`, `core/base58.cpp:24-58`
- Problema: `to_wif()` serializa so a chave privada e opcionalmente `0x01`; depois chama `base58_encode()` direto. Falta prefixo `0x80` e falta `double_sha256` com 4 bytes de checksum.
- Impacto: `wif_compressed` e `wif_uncompressed` estao incorretos.
- Plano:
  - criar `base58check_encode()`
  - gerar WIF como `0x80 || privkey || [0x01] || checksum`
  - cobrir casos comprimido e nao comprimido
- Validacao:
  - vetores de teste de WIF conhecidos
  - round-trip com wallet externa ou script de validacao

## Prioridade 1 - Ganho de velocidade com risco controlado

### P1.1 - `hash8()` otimizado so para 33 bytes
- Arquivo: `core/hash.cpp:215-323`
- Problema: o fast path AVX2 cobre apenas pubkeys comprimidas (`33` bytes). Para `65` bytes, o codigo cai em oito chamadas escalares.
- Impacto: modo `uncompress` e metade do modo `both` perdem throughput de forma desproporcional.
- Plano:
  - implementar caminho vetorizado para `65` bytes
  - medir throughput separado para `compress`, `uncompress` e `both`
  - manter fallback escalar correto
- Validacao:
  - benchmark isolado de hash
  - comparar digest com implementacao escalar

### P1.2 - Multiplicacao ECC duplicada na inicializacao do worker sequencial
- Arquivo: `engine/address.cpp:534`
- Problema: `secp256k1_multiply(current)` e chamado duas vezes para montar `p_jac`.
- Impacto: custo extra por thread na inicializacao; nao e o maior gargalo, mas e desperdicio de CPU caro.
- Plano:
  - calcular uma vez
  - reaproveitar `x` e `y`
- Validacao:
  - benchmark de startup por thread
  - garantir igualdade binaria do ponto inicial

### P1.3 - Afinidade de CPU implementada mas nunca usada
- Arquivos: `system/hardware.cpp:193-200` e workers em `engine/address.cpp`, `engine/bsgs.cpp`, `engine/kangaroo.cpp`
- Problema: ha suporte a pinagem, mas nenhum worker chama isso.
- Impacto: migracao de thread entre cores, perda de cache e oscilacao de throughput.
- Plano:
  - pinar cada worker no proprio `thread_id`
  - tornar isso opcional via flag se necessario
  - remover ou reescrever `pin_all_threads()`, que hoje nao serve para o caso real
- Validacao:
  - benchmark antes/depois
  - medir estabilidade do throughput em janelas longas

## Prioridade 2 - Robustez, limites e consistencia operacional

### P2.1 - Cobertura incompleta no `address` hybrid
- Arquivo: `engine/address.cpp:457-461`
- Problema: `g_hybrid_total_chunks` usa `ceil((end - start) / chunk)` de forma efetiva, mas o range e inclusivo e deveria usar `end - start + 1`.
- Impacto:
  - tecnicamente perde cobertura do ultimo elemento em alguns ranges
  - na pratica, o impacto pode ser de apenas 1 chave em um range enorme
  - ainda assim, contradiz a promessa de cobertura total do modo `hybrid`
- Plano:
  - reescrever a formula para `length = (end - start) + 1`
  - calcular `ceil(length / chunk_size)` com inteiro grande
  - adicionar teste para ranges de 1 elemento, 1 chunk exato e 1 chunk + 1
- Validacao:
  - teste deterministico de cobertura em range pequeno
  - garantir que a contagem de chunks bate com enumeracao exata

### P2.2 - `BSGS` aceita `bits` que o modelo atual nao suporta
- Arquivo: `engine/bsgs.cpp:56-62`
- Problema:
  - `num_baby_steps = 1ULL << b_bits` quebra para `b_bits >= 64`
  - `total_range.limbs[bits/32]` e codigo morto no estado atual; nao afeta a execucao, mas indica modelagem incompleta
- Impacto: comportamento indefinido, overflow ou resultado sem sentido para ranges altos, apesar da CLI aceitar ate `256`.
- Plano:
  - limitar explicitamente o `BSGS` a um teto realista documentado
  - remover `total_range` morto ou corrigir a modelagem se essa variavel voltar a ter funcao
  - se quiser suportar ranges maiores, migrar `num_baby_steps` para BigInt/size policy e rever uso de memoria
- Validacao:
  - testes de fronteira: `bits=40`, `64`, `127`, `128`

### P2.3 - `mul_small_in_place()` esta incorreto para limbs de 64 bits
- Arquivo: `core/secp256k1.cpp:210-217`
- Problema: a funcao trunca cada limb para `uint32_t` e propaga carry de 32 bits.
- Impacto: multiplicacoes pequenas sobre valores grandes podem ser corrompidas. Hoje isso afeta diretamente os saltos de `BSGS` em `engine/bsgs.cpp:132-143`.
- Plano:
  - reimplementar a funcao com aritmetica de 64 bits de ponta a ponta
  - ou remover a funcao e usar multiplicacao segura por escalar pequeno
- Validacao:
  - testes com valores em `limbs[1..3]`
  - comparar contra `operator*`

### P2.4 - `checkpoint_path` e aceito na CLI mas ignorado
- Arquivos: `system/cli.cpp:67-68`, `system/types.hpp:102`
- Problema: o usuario pode passar `-c/--checkpoint`, mas a execucao usa nomes padrao em `engine/address.cpp`.
- Impacto:
  - o comportamento padrao ainda funciona
  - o problema principal e inconsistencia entre a CLI prometida e a execucao real
- Status em `2026-04-29`:
  - `address` ja respeita `options.checkpoint_path`
  - falta fechar a historia nos motores restantes que ainda aceitam a flag pela CLI sem comportamento equivalente
- Plano:
  - plugar `options.checkpoint_path` em todos os motores
  - padronizar precedencia: caminho explicito > caminho default
- Validacao:
  - teste com dois checkpoints simultaneos

### P2.5 - `--benchmark` nao cumpre o contrato documentado
- Arquivos: `system/cli.cpp:73-74`, `system/cli.cpp:215`, `engine/address.cpp:646-665`
- Problema: a ajuda diz que benchmark roda sem checkpoint/FOUND, mas o codigo ainda checkpointa e persiste parte da saida em varios caminhos.
- Impacto: benchmark polui disco e distorce medicao.
- Plano:
  - em benchmark, desabilitar checkpoint, cold boot e escrita de resultados auxiliares
  - unificar o comportamento entre `address`, `BSGS` e `Kangaroo`
- Validacao:
  - rodar benchmark e confirmar zero arquivos novos

### P2.6 - Inicializacoes estaticas nao sao thread-safe
- Arquivos: `core/hash.cpp:25-33`, `core/secp256k1.cpp:594-603`
- Problema: `g_initialized` e `precomputed` sao `bool` comuns acessados por varias threads sem sincronizacao.
- Impacto: data race no primeiro uso sob carga.
- Plano:
  - migrar para `std::once_flag` + `std::call_once`
  - evitar inicializacao preguiçosa sem sincronizacao em hot path multithread
- Validacao:
  - stress test de inicializacao concorrente
  - TSan quando houver toolchain

### P2.7 - Inconsistencia de nomes e caminhos de output
- Arquivos: `engine/address.cpp:291`, `engine/address.cpp:593`, `system/io.cpp:25`, `README.md:130`
- Problema: aparecem `FOUND.txt` e `found.txt`, com formatos diferentes.
- Impacto: logs fragmentados e dificuldade para automacao.
- Plano:
  - unificar nome e formato
  - centralizar escrita em um unico modulo
- Validacao:
  - teste de descoberta em cada motor

## Prioridade 3 - Melhorias estruturais e de longa duracao

### P3.1 - Decomposicao GLV esta incompleta
- Arquivo: `core/secp256k1.cpp` na rotina `decompose_glv()`
- Problema:
  - a implementacao atual cai em um fallback grosseiro para escalares grandes
  - as constantes e o comentario do algoritmo completo existem, mas a decomposicao final nao foi concluida
- Impacto:
  - baixo no estado atual, porque esse caminho nao aparece como hot path dominante nos motores auditados
  - vira problema real se `secp256k1_multiply_glv()` passar a ser integrado ao caminho principal
- Plano:
  - manter como divida tecnica explicita
  - implementar a decomposicao completa so quando o GLV por escalar entrar no hot path ou virar requisito de corretude
  - quando for feito, validar contra vetores conhecidos e comparar com multiplicacao escalar canonica
- Validacao:
  - testes de equivalencia entre `secp256k1_multiply()` e `secp256k1_multiply_glv()` em conjunto de escalares pequenos e grandes

### P3.2 - `lookup_trap_on_disk()` em `Kangaroo` faz scan O(n)
- Arquivo: `engine/kangaroo.cpp:176-198`
- Plano:
  - criar indice em memoria para offsets
  - ou ordenar shards no disco e fazer busca binaria

### P3.3 - Capacidade do Cuckoo Filter do `Kangaroo` e fixa
- Arquivo: `engine/kangaroo.cpp:346-348`
- Plano:
  - dimensionar filtro por RAM disponivel e perfil
  - documentar custo em memoria

### P3.4 - Match final do `address` ainda faz busca linear nos alvos
- Arquivo: `engine/address.cpp:585-600` e bloco equivalente do `hybrid`
- Problema: o Cuckoo Filter barra muita coisa, mas quando ha hit ele faz `for` linear em `matcher.hashes`.
- Impacto:
  - irrelevante para puzzles com 1 alvo
  - passa a importar com listas grandes de alvos
- Plano:
  - manter o Cuckoo como filtro rapido
  - trocar `vector<array<20>>` por estrutura de lookup O(1), por exemplo hash table de 20 bytes
  - preservar uma estrutura separada se a ordem original for importante
- Validacao:
  - benchmark com 1, 1k e 100k alvos

### P3.5 - Build e docs merecem alinhamento de escopo
- Arquivos: `Makefile:9-10`, `Makefile:53-57`, `engine/kangaroo.cpp:411`, `engine/address.cpp:220-244`, `core/hash.cpp:212-323`
- Problema:
  - o build assume `make`, `g++`, `mkdir -p`, `rm -rf`
  - o codigo depende de `__attribute__`, `__builtin_*`, `__uint128_t`
  - hoje isso encaixa melhor em GCC/Clang do que em um build Windows nativo amplo
- Impacto:
  - nao bloqueia uso via WSL/MinGW/GCC
  - vira problema apenas se a meta for suporte nativo amplo fora desse ecossistema
- Plano:
  - alinhar README e docs ao toolchain suportado de fato
  - so depois decidir se vale camada de compatibilidade ou migracao para `CMake`
- Validacao:
  - matriz minima documentada do toolchain oficial

### P3.6 - Falta de suite minima de regressao para corretude criptografica
- Plano:
  - adicionar vetores de teste para:
    - serializacao/deserializacao de pubkey
    - WIF
    - hash160 de pubkeys comprimidas e nao comprimidas
    - scalar multiplication basica
    - GLV/endomorfismo, se mantido

## Ordem Recomendada de Implementacao

1. Corrigir pubkey comprimida e identidade de ponto em `BSGS/Kangaroo`
2. Corrigir `batch_normalize` vs `batch_size`
3. Corrigir encerramento do `address`
4. Corrigir WIF
5. Implementar hash vetorizado para `65` bytes
6. Corrigir `mul_small_in_place` e limites do `BSGS`
7. Corrigir cobertura do `hybrid`
8. Plugar affinity, `checkpoint_path` e comportamento real de benchmark
9. Unificar output/logging
10. Endurecer build multiplataforma

## Protocolo de Benchmark Depois das Correcoes

Medir sempre antes e depois de cada bloco acima:

- `address compress`
- `address uncompress`
- `address both`
- `address hybrid` com `-k 1024`, `4096`, `8192`
- `BSGS` em range pequeno com vetor conhecido
- `Kangaroo` com pubkey conhecida em range curto

Registrar no minimo:

- keys/s ou hops/s
- uso de RAM
- tempo para inicializacao
- tempo para checkpoint
- variacao de throughput por janela de 10s

## Criterios de Aceite

O projeto so deve entrar na fase de tuning pesado depois de cumprir estes gates:

- pubkeys comprimidas validadas em `BSGS` e `Kangaroo`
- nenhuma escrita fora do buffer com `-A max`
- `address` termina corretamente quando exaure range
- `hybrid` cobre 100% do range
- WIF bate com vetores de teste
- `BSGS` rejeita ranges fora do suporte atual ou passa a suportar corretamente

## Resultado Esperado

Depois dessas correcoes, o projeto deve ganhar:

- corretude muito maior nos motores baseados em pubkey
- eliminacao de falso positivo por identidade fraca de ponto
- cobertura confiavel no modo `hybrid`
- mais throughput real em `uncompress` e `both`
- menos variacao de performance sob carga
- build e operacao mais previsiveis

## Checklist de Execução

### Prioridade 0
- [x] P0.1 - Pubkey comprimida desserializada de forma incompleta
- [x] P0.2 - Identidade de ponto reduzida a 64 bits em BSGS e Kangaroo
- [x] P0.4 - Loop principal do `address` nao detecta fim real dos workers
- [x] P0.5 - Overflow potencial em `batch_normalize()`
- [x] P0.6 - WIF esta sendo gerado sem versao e checksum

### Prioridade 1
- [x] P1.1 - `hash8()` otimizado so para 33 bytes
- [x] P1.2 - Multiplicacao ECC duplicada na inicializacao do worker sequencial
- [x] P1.3 - Afinidade de CPU implementada mas nunca usada

### Prioridade 2
- [x] P2.1 - Cobertura incompleta no `address` hybrid
- [x] P2.2 - `BSGS` aceita `bits` que o modelo atual nao suporta
- [x] P2.3 - `mul_small_in_place()` esta incorreto para limbs de 64 bits
- [x] P2.4 - `checkpoint_path` e aceito na CLI mas ignorado
- [x] P2.5 - `--benchmark` nao cumpre o contrato documentado
- [x] P2.6 - Inicializacoes estaticas nao sao thread-safe
- [x] P2.7 - Inconsistencia de nomes e caminhos de output

### Prioridade 3
- [x] P3.1 - Decomposicao GLV esta incompleta
- [x] P3.2 - `lookup_trap_on_disk()` em `Kangaroo` faz scan O(n)
- [x] P3.3 - Capacidade do Cuckoo Filter do `Kangaroo` e fixa
- [x] P3.4 - Match final do `address` ainda faz busca linear nos alvos
- [x] P3.5 - Build e docs merecem alinhamento de escopo
- [x] P3.6 - Falta de suite minima de regressao para corretude criptografica
