# 🚀 Guia de Uso e Operação

Este guia explica como operar o **Bchaves** de forma eficiente em buscas de longa duração.

## 💾 Persistência e Checkpoints

O sistema salva o progresso automaticamente.
- **Formato:** Arquivos `.ckp` binários.
- **Frequência:** A cada 60 segundos (configurável no código).
- **Como retomar:** Basta rodar o mesmo comando novamente. O sistema detecta o arquivo de checkpoint correspondente ao bit-range e retoma a busca.

## 🔍 Tipos de Busca (Modo Address)

O binário `address` suporta três modos de compressão:
1.  **`-R compress`**: Procura apenas por endereços derivados de chaves públicas comprimidas (mais comum).
2.  **`-R uncompress`**: Procura apenas por chaves públicas não-comprimidas (comum em endereços muito antigos).
3.  **`-R both`**: Procura por ambos simultaneamente (padrão, recomendado para puzzles).

## 📂 Organização de Arquivos de Alvo

O Bchaves espera arquivos de texto simples:
- Para `address`: Um endereço por linha (P2PKH).
- Para `bsgs`/`kangaroo`: Uma chave pública em formato HEX por linha.

```bash
# Exemplo de arquivo pubkey.txt
0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

## ⚠️ Dicas de Performance

1.  **Número de Threads (`-t`)**: O padrão é detectar automaticamente, mas para máxima performance em máquinas dedicadas, você pode definir manualmente para o número de núcleos físicos.
2.  **Uso de RAM**: Em modo BSGS, garanta que você tem memória livre suficiente para o filtro. No Kangaroo, o uso de RAM é o que define a probabilidade de colisão (sucesso); quanto mais melhor.
3.  **Dumping em SSD**: Se usar o Kangaroo por longas horas, certifique-se de que o diretório `traps/` está em um SSD/NVMe rápido, para não travar a CPU durante o despejo de dados.
