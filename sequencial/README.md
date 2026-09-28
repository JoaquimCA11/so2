# Versão sequencial

Compile com:

```bash
make
```

Execute informando o número de iterações e a seed:

```bash
./sequencial 10000000 123
```

Nesta versão, um único fluxo calcula as chegadas, atualiza as quatro filas e
libera os carros das direções que estão com sinal verde.

Para apagar o executável:

```bash
make clean
```
