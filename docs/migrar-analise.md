# Análise de `migrar.txt`

## Ajustes necessários para Linux e Windows
- `Makefile` isolado não é suficiente. A base agora usa `CMake` como gerador principal.
- `-march=native`, `-mtune=native` e `-flto` não podem ser obrigatórios por padrão. Eles ficaram atrás da opção `QCHAVES_ENABLE_NATIVE`.
- `pthread_setaffinity_np` é Linux-only. A arquitetura nova isola decisões de hardware em `src/system/hardware.*`.
- Checkpoint e parsing de alvo precisam ser portáveis e independentes de libc específica. A base entregue usa `std::filesystem`, serialização binária simples e CRC32 próprio.
- O plano pressupõe `libsecp256k1` com GLV sempre ativo. Isso continua como objetivo, mas a integração precisa entrar como módulo dedicado e opcional durante o build.

## Melhorias aplicadas na base
- Estrutura de diretórios alinhada ao plano original.
- Três binários separados com parsing de CLI consistente.
- Validação real de Base58Check e public keys.
- Compatibilidade com os arquivos reais existentes em `Puzzles/`.
- Aceite de public key inline para fluxos como `kangaroo`.
- Backend portátil de referência para secp256k1 e busca funcional nos modos `address`, `bsgs` e `kangaroo`.
- Scripts de build/teste em WSL com `g++` quando `cmake` não estiver instalado.
- Camada comum para hardware, métricas e checkpoint.
- Checkpoint com auto-save, resume e save de emergência em `Ctrl+C`.
- Preparação para evolução incremental do core sem quebrar compatibilidade entre plataformas.
