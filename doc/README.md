# 📚 Documentação do Bchaves

Bem-vindo à documentação técnica do **Bchaves**. Este diretório contém detalhes profundos sobre o design, algoritmos e estrutura do sistema.

## 🗂️ Conteúdo

1.  **[Arquitetura do Sistema](architecture.md)**
    *   Explicação das camadas de código (`core`, `engine`, `modulos`).
    *   Fluxo de dependências.
2.  **[Algoritmos e Pesquisa](algorithms.md)**
    *   Funcionamento do BSGS com Cuckoo Filters.
    *   Pollard's Kangaroo e gerenciamento de armadilhas.
    *   Otimizações matemáticas (GLV/Endomorfismo).
3.  **[Estruturas de Dados](data_structures.md)**
    *   Uso de Cuckoo Filters para aceleração.
    *   Implementação de BigInt e Curvas Elípticas.
4.  **[Guia de Uso Avançado](usage.md)**
    *   Configurações de checkpoint.
    *   Gerenciamento de memória e dump para disco.

---

## 🛠️ Visão Geral Rápida

O Bchaves é estruturado para separar **Primitivos Matemáticos** de **Estratégias de Busca**. Isso permite que novos algoritmos sejam implementados na pasta `engine` utilizando as ferramentas já testadas na pasta `core`.

- **Linguagem:** C++17
- **Build System:** Makefile
- **Paralelismo:** Multi-threading nativo (std::thread)
