# Concerns & Technical Debt

## Critical Risks
- **Endianness**: Dependências de ordem de bytes em kernels ARM vs x86 para hashing.

## Mitigated Risks
- **Modular Arithmetic Correctness**: Corrigido na Fase 07 (implementação de redução modular real e propagação de carry em todos os backends SIMD). Necessita monitoramento contínuo via `simd_test`.


## Technical Debt
- **SIMD Duplication**: Código similar repetido entre `sse4`, `avx2` e `avx512` que poderia ser abstraído via wrappers de template (ex: Highway ou XSIMD).
- **Hardware Hardcoding**: Algumas latências de cache e tamanhos de pré-busca ainda estão otimizados para Xeon legado, precisando de maior adaptabilidade para CPUs modernas (Alder Lake, Zen 4).

## Performance Bottlenecks
- **Memory Bandwidth**: O modo Kangaroo com tabelas grandes é limitado pela latência da DDR3 em sistemas legados.
- **Batching Overhead**: O custo de gerenciar lotes de pontos pode eclipsar o ganho da adição paralela se o tamanho do lote for pequeno demais.