# 📚 Documentação do Bchaves

Bem-vindo à documentação técnica do **Bchaves**. Este diretório contém detalhes profundos sobre o design, algoritmos e estrutura do sistema.

## 🗂️ Conteúdo

1.  **[Arquitetura do Sistema](architecture.md)**
    *   Explicação das camadas de código (`core`, `engine`, `modulos`).
    *   Fluxo de dependências.
2.  **[Algoritmos e Pesquisa](algorithms.md)**
    *   **NOVO: Hybrid Chunk Search (LCG Bijection).**
    *   BSGS com Cuckoo Filters e Flat Memory.
    *   Pollard's Kangaroo e Architectural Fleet Model.
    *   Otimizações (GLV, Batch Normalization).
3.  **[Auto-Tune e Hardware](usage.md#A)**
    *   Perfis de tuning inteligente (`safe`, `balanced`, `max`).
    *   Detecção de Cache L3 e CPUID Leaf 4.
4.  **[Guia de Uso Avançado](usage.md)**
    *   Gerenciamento de **Checkpoint v5**.
    *   Gerenciamento de memória e dump para disco.

---

## 🛠️ Visão Geral Rápida

O Bchaves é estruturado para separar **Primitivos Matemáticos** de **Estratégias de Busca**. Isso permite que novos algoritmos sejam implementados na pasta `engine` utilizando as ferramentas já testadas na pasta `core`.

- **Linguagem:** C++17
- **Build System:** Makefile
- **Performance:** Multi-threading, SIMD (AVX2), Auto-Tune
- **Checkpoint:** Versão 5 (Resumível e Atômica)
