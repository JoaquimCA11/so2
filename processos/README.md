# Versão com processos

Compile com:

```bash
make
```

Execute informando carros, tamanho das vias, tempo do semáforo, iterações,
quantidade de processos e seed:

```bash
./processos 100000 100000 30 3000 4 123
```

Troque `4` por `2` ou `8` para mudar a quantidade de processos. O pai cria as
vias, o cruzamento, a barreira e o semáforo numa área de memória compartilhada
(`mmap` com `MAP_SHARED | MAP_ANONYMOUS`) e depois cria os filhos com `fork`.
Cada filho move os carros de um bloco de posições das vias. Um semáforo POSIX
(`sem_wait`/`sem_post`) protege a entrada no cruzamento, e uma barreira
compartilhada entre processos separa as fases de cada iteração.

Para apagar o executável:

```bash
make clean
```
