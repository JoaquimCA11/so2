# Simulação paralela de trânsito em C

Simulação de carros chegando a um cruzamento com semáforos pelas quatro
direções (Norte, Sul, Leste e Oeste). Cada via é dividida em posições; um carro
só avança se a posição da frente estiver livre, forma fila atrás da linha de
parada quando o sinal está vermelho e entra no cruzamento quando o sinal está
verde e o cruzamento está livre.

O projeto tem três versões — sequencial, com processos e com threads — que
recebem a mesma entrada e produzem exatamente o mesmo resultado.

## Dependências

Em Debian ou Ubuntu, instale GCC e Make (e Python 3 para gerar os gráficos,
que usa somente a biblioteca padrão):

```bash
sudo apt update
sudo apt install gcc make python3
```

## Compilação

A partir deste diretório:

```bash
make -C sequencial
make -C processos
make -C threads
```

Cada subdiretório também aceita `make` e `make clean`.

## Entrada e saída

| Parâmetro | Significado |
|---|---|
| `CARROS` | carros distribuídos nas 4 vias no início (no máximo `4 * TAMANHO_VIA`) |
| `TAMANHO_VIA` | número de posições de cada via |
| `TEMPO_SEMAFORO` | iterações que cada eixo (Norte/Sul ou Leste/Oeste) fica verde |
| `ITERACOES` | passos da simulação |
| `TRABALHADORES` | processos ou threads (só nas versões paralelas, de 1 a 256) |
| `SEED` | semente dos sorteios; a mesma seed gera o mesmo resultado |

A saída mostra os carros que atravessaram, os que estão no cruzamento, os que
ainda estão nas vias, o tamanho da fila de cada direção e o tempo de execução.
Sempre vale: `CARROS = atravessaram + no cruzamento + nas vias`.

A entrada usada nas medições está em `testes/entradas/entrada_padrao.txt`.

## Execução

Versão sequencial:

```bash
./sequencial/sequencial 100000 100000 30 3000 123
```

Versão com processos:

```bash
./processos/processos 100000 100000 30 3000 4 123
```

Versão com threads:

```bash
./threads/threads 100000 100000 30 3000 4 123
```

Nas versões paralelas, troque `4` por `2` ou `8` para escolher o número de
trabalhadores. Com essa entrada a versão sequencial leva cerca de 4 segundos
na máquina de testes; aumente `ITERACOES` se levar menos de 2 segundos.

## Testes fora do container

O script compila tudo, executa cada configuração três vezes, confere se todas
as versões dão o mesmo resultado e grava os tempos em
`testes/resultados/tempos_fora_container.csv`:

```bash
./testes/executar_testes.sh
```

Também é possível escolher o arquivo de entrada e o número de repetições:

```bash
./testes/executar_testes.sh testes/entradas/entrada_padrao.txt 5
```

## Docker

Construa a imagem a partir deste diretório:

```bash
docker build -f container/Dockerfile -t trabalho-paralela .
```

Exemplos limitando as CPUs disponíveis:

```bash
docker run --rm --cpus="1" trabalho-paralela ./processos/processos 100000 100000 30 3000 2 123
docker run --rm --cpus="2" trabalho-paralela ./threads/threads 100000 100000 30 3000 4 123
docker run --rm --cpus="4" trabalho-paralela ./threads/threads 100000 100000 30 3000 8 123
```

O script abaixo constrói a imagem e roda as versões com `--cpus` 1, 2 e 4 e
com 2, 4 e 8 trabalhadores, gravando `testes/resultados/tempos_container.csv`:

```bash
./testes/executar_testes_container.sh
```

Se o seu usuário não estiver no grupo `docker`, use
`DOCKER="sudo docker" ./testes/executar_testes_container.sh`.

## Speedup e gráficos

Depois dos testes, gere as tabelas (`resumo.md`) e os gráficos em SVG:

```bash
python3 testes/gerar_graficos.py
```

- `grafico1_tempo.svg`: tempo de execução das três versões;
- `grafico2_speedup.svg`: speedup de processos e threads;
- `grafico3_container.svg`: versões paralelas fora x dentro do container
  (gerado quando existe `tempos_container.csv`).

Mais detalhes estão em `documentacao/explicacao.md` (funcionamento e
paralelização) e `documentacao/analise_resultados.md` (interpretação dos
resultados).
