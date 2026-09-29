# Versão sequencial

Compile com:

```bash
make
```

Execute informando carros, tamanho das vias, tempo do semáforo, iterações e
seed:

```bash
./sequencial 100000 100000 30 3000 123
```

Um único fluxo percorre as iterações. Em cada uma, move os carros das quatro
vias (cada carro anda uma posição se a da frente estiver livre), deixa entrar
no cruzamento o carro da linha de parada das vias com sinal verde e, por fim,
faz os carros de dentro do cruzamento avançarem. Esta versão é a referência de
resultado e de tempo para as versões paralelas.

Para apagar o executável:

```bash
make clean
```
