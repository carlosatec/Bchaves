# Conventions

## Coding Style
- **C++ Moderno**: Uso de `std::optional`, `std::variant`, `std::unique_ptr` e `auto` onde apropriado para clareza.
- **CamelCase/snake_case**: Geralmente segue snake_case para funções e variáveis, e CamelCase para Classes e Structs.
- **RAII**: Gerenciamento de recursos (arquivos, memória, mutexes) via escopo.

## SIMD Guidelines
- **Portabilidade**: Funções SIMD devem sempre ter um fallback escalar ou estar protegidas por `#ifdef`.
- **Limb Management**: Números de 256 bits são representados como arrays de 4 limbs de 64 bits.

## Error Handling
- **No Exceptions**: O projeto evita o uso de exceções C++ em caminhos de performance crítica, preferindo códigos de retorno ou status enum.
- **Assertions**: Uso extensivo de `assert()` em builds de debug para validar invariantes matemáticas.