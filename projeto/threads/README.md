# Versão com threads

Compile com:

```bash
make
```

Execute informando iterações, quantidade de threads e seed:

```bash
./threads 10000000 4 123
```

Troque `4` por `2` ou `8` para mudar a quantidade de threads. As threads
calculam blocos diferentes das chegadas em um vetor compartilhado. Um mutex
protege a soma compartilhada dos carros gerados.

Para apagar o executável:

```bash
make clean
```
