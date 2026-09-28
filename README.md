# Simulação paralela de trânsito em C

O projeto compara uma implementação sequencial com implementações que usam
processos e threads. Todas recebem a mesma seed e produzem exatamente o mesmo
estado final da simulação.

## Dependências

Em Debian ou Ubuntu, instale GCC e Make:

```bash
sudo apt update
sudo apt install gcc make
```

## Compilação

A partir deste diretório:

```bash
make -C sequencial
make -C processos
make -C threads
```

Cada subdiretório também aceita `make` e `make clean`.

## Execução

Versão sequencial:

```bash
./sequencial/sequencial 10000000 123
```

Versão com processos:

```bash
./processos/processos 10000000 4 123
```

Versão com threads:

```bash
./threads/threads 10000000 4 123
```

Nas versões paralelas, troque `4` por `2` ou `8` para escolher o número de
trabalhadores. Aumente `10000000` se a versão sequencial ainda levar menos de
dois segundos na máquina usada para a apresentação.

As versões paralelas guardam quatro bytes de chegadas por iteração. Por
exemplo, 10 milhões de iterações usam aproximadamente 40 MB para essa tabela.

## Testes e CSV

O script compila tudo, executa cada configuração três vezes, confere a
equivalência dos resultados e grava os tempos reais em `testes/resultados.csv`:

```bash
./testes/executar_testes.sh
```

Também é possível escolher iterações, seed e repetições:

```bash
./testes/executar_testes.sh 20000000 123 5
```

## Docker

Construa a imagem a partir deste diretório:

```bash
docker build -f container/Dockerfile -t trabalho-paralela .
```

Exemplos limitando as CPUs disponíveis:

```bash
docker run --rm --cpus="1" trabalho-paralela ./sequencial/sequencial 10000000 123
docker run --rm --cpus="2" trabalho-paralela ./threads/threads 10000000 2 123
docker run --rm --cpus="4" trabalho-paralela ./processos/processos 10000000 4 123
docker run --rm --cpus="8" trabalho-paralela ./threads/threads 10000000 8 123
```

Mais detalhes estão em `documentacao/explicacao.md` e nos READMEs de cada
versão.
