# Architecture

## Design Patterns
- **SIMD Dispatching**: O sistema utiliza dispatch estático-performance (runtime selection via ponteiros de função) para selecionar o kernel mais rápido (AVX512, AVX2, SSE4, NEON) sem overhead de branching em loops quentes.
- **Worker/Orchestrator**: O `App` orquestra múltiplas instâncias de `Worker` (threads), cada uma operando de forma independente com sincronização mínima para maximizar o throughput.
- **Batched Point Addition**: Uso de Montgomery Batch Inversion (quando aplicável) e fórmulas Jacobianas para acelerar a adição de pontos em massa.

## Data Flow
1. **Hardware Detection**: No início, o sistema detecta as capacidades da CPU.
2. **Context Initialization**: Setup do gerador de pontos e tabelas de pré-computação.
3. **Search Loop**: As threads executam o algoritmo selecionado (Kangaroo/BSGS/Address).
4. **Checkpointing**: O progresso é salvo periodicamente ou em sinais de interrupção (SIGINT).
5. **Collision Detection**: Uso de `Cuckoo Filter` ou tabelas de hash (FlatMap) para detecção rápida de colisões.

## Persistence Model
- Arquivos de checkpoint binários para recuperação instantânea.
- Sistema de "Cold Boot" para carregar tabelas grandes da memória persistente para RAM.