# Análise dos resultados

Medições feitas em 29/09/2026, fora do container, com
`./testes/executar_testes.sh` (3 execuções por configuração). Os valores
completos estão em `testes/resultados/resumo.md` e nos gráficos 1 e 2.

## Ambiente e entrada

- Processador: AMD Ryzen 5 PRO 3500U — 4 núcleos físicos, 2 threads por
  núcleo (SMT), 8 CPUs lógicas; notebook com limite de energia.
- Entrada: `CARROS=100000 TAMANHO_VIA=100000 TEMPO_SEMAFORO=30
  ITERACOES=3000 SEED=123` (400 mil posições atualizadas por iteração).
- Todas as execuções produziram exatamente o mesmo resultado da sequencial.

## Resultados

| Trabalhadores | Sequencial (s) | Processos (s) | Threads (s) | Speedup processos | Speedup threads |
|---|---|---|---|---|---|
| 1 | 4,29 | 4,35 | 4,37 | 0,99 | 0,98 |
| 2 | — | 2,78 | 2,78 | 1,55 | 1,54 |
| 4 | — | 1,70 | 1,72 | 2,52 | 2,49 |
| 8 | — | 1,37 | 1,36 | 3,13 | 3,15 |

## Interpretação

### 1. O paralelismo funciona, mas o ganho é menor que o ideal

O tempo cai de 4,29 s para 1,36 s com 8 trabalhadores (speedup 3,15). A
eficiência (speedup ÷ trabalhadores) cai de 77% com 2 para 39% com 8. Os
fatores abaixo foram **medidos** nesta máquina e explicam a maior parte da
diferença.

**Frequência do processador.** Num notebook, o processador acelera um núcleo
sozinho e reduz a frequência quando vários trabalham. Durante as execuções:

| Situação | Frequência medida |
|---|---|
| Sequencial (1 núcleo) | ~3,7 GHz |
| 2 threads | ~3,35 GHz |
| 4 threads | ~2,9 GHz |
| 8 threads | ~2,7 GHz |

Só por isso, com 4 trabalhadores cada um já roda cerca de 20% mais devagar que
a versão sequencial.

**Núcleos físicos x lógicos (SMT).** São 4 núcleos físicos. Duas threads no
mesmo núcleo dividem as unidades de execução:

| Teste (threads fixadas com `taskset`) | Tempo |
|---|---|
| 2 threads em núcleos físicos diferentes | ~2,5 s (speedup ~1,7) |
| 2 threads no mesmo núcleo físico | ~3,0 s (speedup ~1,4) |
| 4 threads, uma por núcleo físico | 1,68 s |
| 8 threads (todas as CPUs lógicas) | 1,31 s |

Sem fixar, o escalonador às vezes coloca as duas threads no mesmo núcleo, o que
explica o speedup médio de 1,55 com 2 trabalhadores e a variação maior entre
as execuções (desvio de ~0,1 s). Passar de 4 para 8 trabalhadores ainda ajuda
(cerca de 28%), mas não dobra, porque os núcleos físicos continuam sendo 4.
Medida pelos núcleos físicos, a eficiência com 8 é 3,15 ÷ 4 ≈ 79%.

**Barreiras.** Cada iteração tem 2 barreiras, ou seja, 6000 por execução. Um
teste isolado mediu cerca de 10 µs por barreira com 2 threads e 15 µs com 8.
Isso dá ~0,06 s com 2 trabalhadores e ~0,09 s com 8 — cerca de 2% a 7% do
tempo paralelo. O custo cresce com o número de trabalhadores, enquanto o
trabalho de cada um diminui.

**Outros fatores menores.** A fase do cruzamento é feita por um trabalhador só
(é pequena: 4 células). Na barreira, todos esperam o mais lento; os blocos não
têm custo idêntico, porque posições vazias são mais baratas que posições com
carros e o começo das vias esvazia com o tempo. Os núcleos também dividem a
cache L3 e o acesso à memória.

### 2. Processos e threads tiveram o mesmo desempenho

As diferenças ficaram abaixo de 2%, menores que a variação entre execuções. É o
esperado para este programa:

- as duas versões executam o mesmo código de cálculo;
- as duas usam uma barreira da biblioteca pthreads (na de processos, marcada
  como compartilhada) e o mesmo número de sincronizações;
- a memória compartilhada criada com `mmap` é memória comum; depois de criada,
  acessá-la custa o mesmo que acessar memória alocada com `malloc`;
- `fork` é mais caro que `pthread_create`, mas acontece só 8 vezes numa
  execução de mais de 1 segundo.

Processos seriam mais lentos se houvesse muita criação e destruição de
trabalhadores, ou se os dados precisassem ser copiados entre eles (por pipes ou
mensagens). Aqui nada disso acontece.

### 3. Com 1 trabalhador, as versões paralelas custam ~1–2% a mais

Esse é o custo da estrutura paralela sem nenhum ganho: criar o trabalhador e
passar pelas barreiras. É pequeno porque há muito trabalho entre uma barreira e
outra.

### 4. Cuidado de medição: o compilador

Nas primeiras medições, a versão com processos era ~15% mais lenta mesmo com 1
trabalhador. A causa não era processos x threads: o GCC tinha colado o laço
principal dentro do `main` (inline) só na versão com processos, gerando um
código mais lento. Com o laço mantido como uma função separada nas três versões
(`__attribute__((noinline))`), a diferença desapareceu. A lição: antes de
atribuir uma diferença de tempo ao paralelismo, é preciso garantir que as
versões executam o mesmo código.

### 5. Granularidade

As vias são longas (100 mil posições) de propósito. Com vias curtas, cada
trabalhador faria pouco trabalho entre as barreiras e o custo fixo de ~10–15 µs
por barreira dominaria o tempo. Um bom experimento extra para a apresentação é
reduzir `TAMANHO_VIA` e aumentar `ITERACOES` mantendo o total de trabalho, e
observar o speedup cair.

## Container

Medições feitas em 29/09/2026 com `./testes/executar_testes_container.sh` (3
execuções por configuração). Tabela completa em `testes/resultados/resumo.md`;
gráfico 3. Todas as execuções deram o mesmo resultado da sequencial.

Tempo médio em segundos (processos / threads):

| Trabalhadores | Fora | `--cpus=1` | `--cpus=2` | `--cpus=4` |
|---|---|---|---|---|
| Sequencial | 4,29 | 4,42 | 4,64 | 4,50 |
| 2 | 2,78 / 2,78 | 6,08 / 6,18 | 3,08 / 3,20 | 3,08 / 2,89 |
| 4 | 1,70 / 1,72 | 11,73 / 11,53 | 4,15 / 3,98 | 1,78 / 1,72 |
| 8 | 1,37 / 1,36 | 16,23 / 15,42 | 5,73 / 6,17 | 2,50 / 2,42 |

### 1. O container em si custa pouco

Quando há CPU liberada para todos os trabalhadores, o tempo dentro fica perto
do tempo fora: a sequencial ficou 3% a 8% mais lenta; 4 trabalhadores com
`--cpus=4` ficaram entre 0% e 5% acima. Um container não é uma máquina virtual:
o programa roda direto no mesmo núcleo do Linux, apenas isolado por namespaces
e limitado por cgroups. Parte dessa pequena diferença pode ser variação de
temperatura e frequência, já que os testes no container rodaram por vários
minutos seguidos num notebook; com estes dados não dá para separar as duas
coisas.

### 2. O melhor número de trabalhadores é o número de CPUs liberadas

- `--cpus=2`: o melhor tempo foi com 2 trabalhadores (~3,1 s, speedup ~1,5);
- `--cpus=4`: o melhor foi com 4 trabalhadores (~1,75 s, speedup ~2,5), igual
  a fora do container.

### 3. Trabalhadores demais pioram muito — ficam piores que a sequencial

Com `--cpus=1`, 8 trabalhadores levaram ~16 s, cerca de 3,5 vezes o tempo
da sequencial. Com `--cpus=2`, 8 trabalhadores (~6 s) também perderam para a
sequencial. Dois motivos:

**a) `--cpus` limita tempo de CPU, e o paralelo gasta mais tempo de CPU.** O
limite não diz em quais núcleos o container roda; ele dá uma cota de tempo de
CPU a cada período de 100 ms (100 ms com `--cpus=1`, 200 ms com `--cpus=2`).
Fora do container, medimos com `time` o tempo de CPU somado de todos os
trabalhadores:

| Execução | Tempo de relógio | Tempo de CPU somado |
|---|---|---|
| Sequencial | 4,26 s | 4,26 s |
| 2 trabalhadores | ~2,85 s | ~5,2 s |
| 4 trabalhadores | ~1,73 s | ~6,2 s |
| 8 trabalhadores | ~1,26 s | ~8,4 s |

O paralelo termina antes, mas gasta mais CPU no total: cada núcleo roda mais
devagar quando vários trabalham, duas threads no mesmo núcleo físico dividem as
unidades de execução, e as barreiras têm custo. Com cota de 1 CPU, o tempo de
relógio não pode ser menor que o tempo de CPU necessário. Só isso já explica
boa parte do resultado com 2 trabalhadores (5,2 s de CPU → 6,1 s no container).

**b) Pausas por esgotamento da cota (throttling).** Com 8 trabalhadores e
`--cpus=1`, os 8 rodam ao mesmo tempo em 8 CPUs e consomem os 100 ms de cota em
cerca de 12,5 ms; depois o grupo inteiro fica parado até o próximo período. Com
duas barreiras por iteração, a simulação só avança quando todos os
trabalhadores conseguem rodar, e qualquer pausa no meio de uma fase atrasa
todos. Por isso o tempo com 4 e 8 trabalhadores (11,6 e ~16 s) fica bem acima
até do tempo de CPU medido fora (6,2 e 8,4 s). A variação também aumenta: o
desvio das threads com 8 trabalhadores e `--cpus=1` foi de 1,1 s.

Para mostrar as pausas na apresentação, rode o comando abaixo e observe
`nr_throttled` (quantas vezes a cota acabou) e `throttled_usec` (tempo parado).
Ele não foi executado durante a preparação deste texto:

```bash
sudo docker run --rm --cpus=1 trabalho-paralela sh -c \
  './threads/threads 100000 100000 30 3000 8 123 | tail -1; cat /sys/fs/cgroup/cpu.stat'
```

### 4. Processos e threads continuam equivalentes

Dentro do container as duas versões ficaram próximas em todas as combinações,
sem uma vencedora consistente: ora processos, ora threads foram um pouco mais
rápidos, dentro da variação entre execuções. A conclusão da seção 2 continua
valendo com limite de CPU.

### 5. Consequência prática

`--cpus` não muda o número de CPUs que o programa enxerga: dentro do container,
`nproc` continua mostrando todas as CPUs da máquina (confira com
`sudo docker run --rm --cpus=1 trabalho-paralela nproc`). Um programa que escolhesse o número de
trabalhadores pelo número de CPUs visíveis criaria trabalhadores demais e
ficaria mais lento. Em ambiente com limite de CPU, o número de trabalhadores
deve seguir o limite (aqui, o parâmetro de linha de comando permite isso).

## Conclusões

1. As três versões produzem exatamente o mesmo resultado; a paralelização não
   mudou a simulação.
2. Com 8 trabalhadores o tempo caiu de 4,29 s para ~1,36 s (speedup ~3,1) numa
   máquina de 4 núcleos físicos. O ganho é limitado pela queda de frequência,
   pelo SMT e pelas barreiras de cada iteração.
3. Processos com memória compartilhada e threads tiveram o mesmo desempenho,
   porque executam o mesmo cálculo com a mesma sincronização; a diferença entre
   eles está em como a memória é compartilhada, não no custo de acessá-la.
4. Em container, o isolamento custa pouco, mas o limite de CPU manda: com mais
   trabalhadores do que CPUs liberadas, o programa fica mais lento até que a
   versão sequencial.
