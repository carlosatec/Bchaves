# Coding Conventions

## Style Guide
- **Naming**:
    - Types/Structs: `PascalCase` (e.g., `PointJacobian`).
    - Functions/Variables: `snake_case` (e.g., `add_points`).
    - Constants: `k` prefix followed by `PascalCase` (e.g., `kFieldPrime`).
    - Namespaces: `snake_case` (e.g., `bchaves::core`).
- **Formatting**:
    - Indentation: 4 spaces.
    - Braces: Attached to the same line for `if`, `for`, `while`, `namespace`.
- **Headers**:
    - Always use `#pragma once`.
    - Group includes: Standard library first, then internal headers.

## Error Handling
- Use `int` return codes (0 for success) for major engine functions.
- Pass `std::string& error` for detailed error reporting in configuration functions.
- Assertions used in performance-critical kernels (where appropriate).

## Resource Management
- Prefer RAII and smart pointers (`std::unique_ptr`, `std::shared_ptr`).
- Avoid manual `new`/`delete` in high-level code.
- Align performance-critical data to 32/64 bytes for SIMD friendliness.

## Documentation
- Doxygen-style comments for public API headers (`.hpp`).
- File headers with project name, description, author, and license.
