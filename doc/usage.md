# 🚀 Guia de Uso e Operação

Este guia explica como operar o **Bchaves** de forma eficiente em buscas de longa duração.

## 💾 Persistência e Checkpoints

O sistema salva o progresso automaticamente de forma segura.
- **Formato:** Arquivos `.ckp` binários (mais compactos e resistentes a erros).
- **Frequência:** A cada 60 segundos por padrão.
- **Salvamento de Emergência:** Ao capturar um sinal de interrupção (`Ctrl+C`), o Bchaves tenta salvar o estado atual antes de encerrar as threads.
- **Como retomar:** Basta rodar o mesmo comando. O sistema detecta o checkpoint e retoma a busca.

## 🔍 Tipos de Busca (Modo Address)

A busca automática identifica três tipos principais de endereços:
1.  **P2PKH (Legacy)**: Endereços que começam com `1`.
2.  **P2SH (Nested SegWit)**: Endereços que começam com `3`.
3.  **Bech32 (Native SegWit)**: Endereços que começam com `bc1q` (v0).

O binário `address` também permite filtrar por compressão:
1.  **`-l compress`**: Procura apenas por chaves comprimidas.
2.  **`-l uncompress`**: Procura apenas por chaves não-comprimidas.
3.  **`-l both`**: Procura por ambos simultaneamente (padrão).

## 📂 Organização de Arquivos de Alvo

O Bchaves espera arquivos de texto simples:
- Para `address`: Um endereço por linha (P2PKH).
- Para `bsgs`/`kangaroo`: Uma chave pública em formato HEX por linha.

```bash
# Exemplo de arquivo pubkey.txt
0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

## 🖥️ Verificando seu Hardware

O Bchaves permite validar se as otimizações de CPU estão ativas:
```bash
./build/address --list-hardware
```
Isso exibirá a contagem de cores, memória livre, cache L3 e suporte a **AVX2 / BMI2 / SHA-NI**.

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
