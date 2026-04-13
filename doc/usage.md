# 🚀 Guia de Uso e Operação

Este guia explica como operar o **Bchaves** de forma eficiente em buscas de longa duração.

## 💾 Persistência e Checkpoints

O sistema salva o progresso automaticamente de forma segura em arquivos binários **Versão 5**.
- **Formato:** O `.ckp` v5 agora armazena o estado exato dos chunks (`counter`, `step`, `size`), permitindo retomada determinística em buscas pseudoaleatórias.
- **Incompatibilidade**: Checkpoints v3/v4 não são mais compatíveis. É necessário iniciar novas buscas ou deletar arquivos antigos se houver erro de versão.
- **Frequência:** Geralmente a cada 10-60 segundos (configurável via CLI).
- **Salvamento de Emergência:** Ao capturar `Ctrl+C`, o motor gera um checkpoint instantâneo compensando as threads em processamento.

A busca identifica P2PKH, P2SH e Bech32. Além disso, o motor oferece quatro estratégias de exploração:

1.  **`-R hybrid`**: **(Recomendado)** Usa bijeção LCG para varrer o range de forma desordenada mas completa (100% de cobertura).
    -   **Uso de `-k`**: Controla o tamanho do "chunk". Ex: `-k 4096` cria blocos de 4 milhões de chaves, o que é ideal para minimizar o custo fixo de multiplicação ECC na GPU/CPU.
2.  **`-R sequential/backward`**: Busca linear para ranges muito curtos onde o overhead do LCG não se justifica.
3.  **Filtragem de Alvos (`-l`)**: `compress`, `uncompress` ou `both`.

## 📂 Organização de Arquivos de Alvo

O Bchaves espera arquivos de texto simples:
- Para `address`: Um endereço por linha (P2PKH).
- Para `bsgs`/`kangaroo`: Uma chave pública em formato HEX por linha.

```bash
# Exemplo de arquivo pubkey.txt
0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

## ⚙️ Perfis de Hardware (-A)

O Bchaves introduz o **Auto-Tune**, que configura o motor conforme o perfil de uso:
- **`-A safe`**: Ideal para notebooks. Usa 50% dos núcleos físicos e lotes menores para manter a temperatura estável e o PC responsivo.
- **`-A balanced`**: (Padrão) Usa 100% dos núcleos físicos. Ideal para servidores compartilhados.
- **`-A max`**: Força bruta. Ativa Hyper-Threading (núcleos lógicos) e lotes massivos de memória para extrair o máximo de MH/s.

---

## 🖥️ Verificando seu Hardware

O Bchaves permite validar se as otimizações de CPU estão ativas:
```bash
./build/address --list-hardware
```
Exibe cores térmicos, RAM livre e extensões **AVX2 / SHA-NI / BMI2**.

## 🚀 Kangaroo Simplificado com Bits

Antigamente o Kangaroo exigia ranges em Hexadecimal. Agora você pode usar bits:
```bash
# Busca automática entre 2^74 e 2^75-1
./build/kangaroo targets.txt -b 75 -t 12
```

## ⚠️ Dicas de Performance

1.  **Número de Threads (`-t`)**: O padrão é detectar automaticamente, mas para máxima performance em máquinas dedicadas, você pode definir manualmente para o número de núcleos físicos.
2.  **Uso de RAM**: Em modo BSGS, agora usamos apenas **16 bytes** por ponto. Uma máquina com 16GB de RAM pode carregar quase 1 bilhão de Baby Steps.
3.  **Dumping em SSD**: Se usar o Kangaroo por longas horas, certifique-se de que o diretório `traps/` está em um SSD/NVMe rápido, para não travar a CPU durante o despejo de dados.
