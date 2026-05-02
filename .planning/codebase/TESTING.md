# Testing Strategy

## Unit Tests
- **Crypto Kernel**: Validação de todas as operações de Secp256k1 (adição, doubling, multiplicação escalar) contra vetores de teste conhecidos e a implementação `secp256k1.c` (libsecp256k1).
- **SIMD Validation**: Teste específico para garantir que cada instrução intrínseca produz o mesmo resultado que a lógica escalar correspondente.

## Integration Tests
- **End-to-End Search**: Execução de buscas em intervalos pequenos conhecidos para garantir que o motor encontra a chave privada correta.
- **Persistence Test**: Ciclos de interrupção e retomada para validar a integridade dos checkpoints.

## Benchmarking
- **Throughput Tracks**: Medição de Keys per Second (K/s) ou Million Keys per Second (M/s) para cada backend.
- **Heat Analysis**: Identificação de gargalos em aritmética modular vs hashing vs memória.