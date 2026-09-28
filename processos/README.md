# Versão com processos

Compile com:

```bash
make
```

Execute informando iterações, quantidade de processos e seed:

```bash
./processos 10000000 4 123
```

Troque `4` por `2` ou `8` para mudar a quantidade de processos. Os processos
calculam blocos diferentes das chegadas e escrevem em memória compartilhada.
Um semáforo POSIX protege a soma compartilhada dos carros gerados.

Para apagar o executável:

```bash
make clean
```
