# Versão com threads

Compile com:

```bash
make
```

Execute informando carros, tamanho das vias, tempo do semáforo, iterações,
quantidade de threads e seed:

```bash
./threads 100000 100000 30 3000 4 123
```

Troque `4` por `2` ou `8` para mudar a quantidade de threads. As threads
compartilham naturalmente a memória do processo: cada uma move os carros de um
bloco de posições das vias. Um `pthread_mutex_t` protege a entrada no
cruzamento, e uma `pthread_barrier_t` separa as fases de cada iteração.

Para apagar o executável:

```bash
make clean
```
