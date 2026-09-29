# Explicação do projeto

## 1. Proposta e o que foi implementado

| Proposta | Implementação |
|---|---|
| Carros vindos de diferentes direções | 4 vias: Norte, Sul, Leste e Oeste |
| Formar filas e avançar conforme o semáforo e a posição dos outros carros | cada via é um vetor de posições; o carro só anda se a posição da frente estiver livre e para na linha de parada com sinal vermelho |
| Entrada: quantidade de carros, tamanho das vias, tempo dos semáforos, iterações, processos/threads | `CARROS TAMANHO_VIA TEMPO_SEMAFORO ITERACOES [TRABALHADORES] SEED` |
| Saída: carros que atravessaram, tamanho das filas, tempo | carros que atravessaram, no cruzamento e nas vias, fila de cada direção e tempo |
| Vias processadas por processos ou threads diferentes | as posições das 4 vias são divididas em blocos, um por trabalhador (com 4 trabalhadores, uma via para cada) |
| Sincronizar o acesso ao cruzamento | entrada no cruzamento protegida por mutex (threads) ou semáforo (processos), e barreiras entre as fases de cada iteração |
| Memória compartilhada na versão com processos | vias, cruzamento, semáforo e barreira ficam numa área criada com `mmap` |

A `SEED` foi acrescentada para que os sorteios sejam repetíveis e as três
versões possam ser comparadas.

## 2. O modelo

### Vias

Cada via tem `TAMANHO_VIA` posições. Uma posição vale 0 (vazia) ou 1 (carro).
A posição 0 é o começo da via e a última posição é a **linha de parada**, colada
ao cruzamento. No início, `CARROS / 4` carros são colocados em posições
sorteadas de cada via.

Em cada iteração, um carro que não está na linha de parada:

- anda uma posição se a posição da frente estiver livre;
- com 10% de chance, o motorista hesita e fica parado mesmo com espaço
  (`CHANCE_HESITAR`). Isso vem do modelo de tráfego de Nagel-Schreckenberg e
  deixa o trânsito menos artificial.

Como um carro só anda para uma posição vazia, carros parados na frente fazem os
de trás pararem: é assim que a fila se forma.

### Semáforo

O eixo Norte/Sul fica verde durante `TEMPO_SEMAFORO` iterações, depois o eixo
Leste/Oeste fica verde pelo mesmo tempo, e assim por diante (`sinal_verde`).

### Cruzamento

O cruzamento tem 2 x 2 células, e o trânsito anda pela mão direita:

```text
             Norte ↓
             ┌────┬────┐
             │ NO │ NE │ ← Leste
             ├────┼────┤
     Oeste → │ SO │ SE │
             └────┴────┘
                    ↑ Sul
```

| Via | 1ª célula | 2ª célula |
|---|---|---|
| Norte (desce) | NO | SO |
| Sul (sobe) | SE | NE |
| Leste (vai para oeste) | NE | NO |
| Oeste (vai para leste) | SO | SE |

Vias do mesmo eixo não usam células em comum; vias de eixos diferentes, sim.
O carro da linha de parada entra no cruzamento quando o sinal da sua via está
verde **e** a primeira célula do seu caminho está livre.

Na troca de sinal isso importa. Se um carro do Norte entrou na última iteração
verde, ele ainda está em SO; o carro do Oeste, que acabou de ganhar o verde,
quer entrar justamente em SO e precisa esperar uma iteração. Quem já está dentro
do cruzamento tem preferência, e dois carros nunca ocupam a mesma célula.

### Fila e contagem final

A fila de uma direção é a quantidade de carros encostados um atrás do outro a
partir da linha de parada. Ao final sempre vale:

```text
CARROS = carros que atravessaram + carros no cruzamento + carros nas vias
```

## 3. Uma iteração

Todas as versões fazem a mesma coisa, na mesma ordem:

1. **Fase 1 — vias:** calcula a nova posição de cada carro. O carro da linha de
   parada tenta entrar no cruzamento (`tentar_entrar_no_cruzamento`).
2. **Fase 2 — cruzamento:** carros na segunda célula saem (contam como
   atravessados) e carros na primeira passam para a segunda
   (`avancar_cruzamento`).

### Dois estados das vias

As vias ficam duas vezes na memória: o estado **atual** e o da **próxima**
iteração. Na iteração `t`, lê-se o estado `t % 2` e escreve-se o `(t + 1) % 2`.
Na iteração seguinte os papéis se invertem.

A nova ocupação de uma posição depende só do estado atual da posição anterior,
dela mesma e da seguinte (`atualizar_trecho`). Por isso:

- cada trabalhador escreve **somente** nas próprias posições; nunca há duas
  escritas no mesmo lugar;
- ninguém lê um valor que outro trabalhador está alterando naquela iteração.

Se houvesse um só vetor, um carro poderia andar duas vezes na mesma iteração ou
o resultado dependeria da ordem em que os blocos são processados.

### Sorteios determinísticos

`sortear(seed, iteração, via, posição)` mistura esses números com
multiplicações e deslocamentos de bits e devolve sempre o mesmo valor para os
mesmos argumentos. A decisão "este motorista hesita?" não depende de qual
trabalhador faz a conta nem de quando. É isso que permite que sequencial,
processos e threads cheguem exatamente ao mesmo resultado.

## 4. Versão sequencial

Um único fluxo chama `atualizar_bloco` para todas as posições (de 0 a
`4 * TAMANHO_VIA`) e depois `avancar_cruzamento`, em cada iteração. É a
referência de resultado e de tempo.

## 5. Estratégia de paralelização

### Divisão do trabalho

As quatro vias ficam em sequência na memória: `[Norte][Sul][Leste][Oeste]`. As
`4 * TAMANHO_VIA` posições são divididas em blocos contíguos de tamanhos quase
iguais (`inicio_do_bloco`); quando a divisão tem resto, os primeiros blocos
ganham uma posição a mais.

- 2 trabalhadores: um fica com Norte e Sul, o outro com Leste e Oeste;
- 4 trabalhadores: uma via para cada;
- 8 trabalhadores: meia via para cada.

O custo está em mover os carros: são `4 * TAMANHO_VIA` posições por iteração.
A fase 2 mexe em apenas 4 células e é feita por um único trabalhador (o de
número 0).

### Comunicação

Na fronteira entre dois blocos, o trabalhador precisa saber se o carro na
última posição do bloco anterior vai entrar no seu bloco, e se a posição logo
depois do seu bloco está livre. Ele lê essas posições diretamente do estado
atual, que está na memória compartilhada. Esse é o ponto de comunicação entre
os trabalhadores em cada iteração.

### Sincronização: barreiras

Cada iteração tem duas barreiras:

```text
fase 1 (todos, em paralelo) → barreira → fase 2 (só o 0) → barreira → próxima iteração
```

- **Primeira barreira:** a fase 2 só pode começar depois que todos terminaram a
  fase 1, porque precisa ver todas as entradas no cruzamento. Ela também
  garante que ninguém ainda está lendo o estado atual quando ele começar a ser
  sobrescrito, na iteração seguinte (os dois estados trocam de papel).
- **Segunda barreira:** ninguém começa a próxima iteração, e tenta entrar no
  cruzamento, antes de os carros de dentro do cruzamento andarem.

Com uma barreira só, a fase 2 de uma iteração rodaria ao mesmo tempo que a
fase 1 da seguinte, e as entradas no cruzamento dependeriam de quem chegou
primeiro.

### Região crítica: entrada no cruzamento

`tentar_entrar_no_cruzamento` verifica se a célula está livre, ocupa a célula e
incrementa `carros_no_cruzamento`. Com o sinal verde para Norte e Sul, os
trabalhadores donos dessas duas linhas de parada podem executar esse trecho ao
mesmo tempo. Sem exclusão mútua:

- `carros_no_cruzamento++` é ler, somar e gravar. Se dois trabalhadores leem 3
  ao mesmo tempo, os dois gravam 4 e uma entrada se perde;
- "verificar e depois ocupar" deixaria duas entradas passarem pela verificação
  antes de qualquer uma ocupar a célula.

Por isso o trecho fica entre `pthread_mutex_lock`/`unlock` (threads) ou
`sem_wait`/`sem_post` (processos).

**Por que o mutex não deixa o resultado aleatório?** Em uma iteração só o eixo
verde pode entrar; as duas vias desse eixo usam células diferentes; e o
contador é uma soma, que dá o mesmo valor em qualquer ordem. Quem decide se o
carro entra é a regra (sinal verde + célula livre), não a ordem em que os
trabalhadores pegam o mutex. Se deixássemos vias de eixos diferentes
disputarem a mesma célula "no mutex", o vencedor dependeria do escalonador e as
execuções dariam resultados diferentes.

## 6. Versão com processos

1. O pai cria com `mmap(..., MAP_SHARED | MAP_ANONYMOUS, ...)` uma área com
   `DadosCompartilhados`: a barreira, o cruzamento (com o semáforo e os
   contadores) e os dois estados das vias. Memória criada assim continua
   compartilhada depois do `fork`; memória comum (pilha, `malloc`) seria
   copiada, e o que um filho alterasse não apareceria para os outros.
2. `sem_init(&mutex, 1, 1)`: o `1` do meio indica semáforo compartilhado entre
   processos; o valor inicial 1 faz ele funcionar como mutex (semáforo binário).
3. A barreira é uma `pthread_barrier_t` com o atributo
   `PTHREAD_PROCESS_SHARED`, também guardada na área compartilhada.
4. O pai distribui os carros e cria os filhos com `fork()`. Cada filho executa
   `trabalho_do_processo` no seu bloco e termina com `_exit`.
5. Os parâmetros (`Parametros`) não mudam durante a simulação, então cada filho
   usa a cópia recebida no `fork`; só o que muda fica na área compartilhada.
6. O pai espera todos com `waitpid` e lê o resultado da área compartilhada.

Se um `fork` falhar, os filhos já criados ficariam presos para sempre na
barreira esperando os que não existem; por isso o pai os encerra com `kill`.

## 7. Versão com threads

1. As vias são alocadas com `malloc`; threads do mesmo processo já enxergam a
   mesma memória.
2. O `Cruzamento` guarda o próprio `pthread_mutex_t`, junto do dado que ele
   protege.
3. A barreira é criada para o número de threads trabalhadoras.
4. `pthread_create` inicia cada thread com seu `id`; a principal espera todas
   com `pthread_join`.

Se a criação de uma thread falhar, as já criadas ficariam presas na barreira;
o programa então termina o processo inteiro com `exit`.

## 8. Processos e threads

Um processo tem o próprio espaço de endereçamento: para compartilhar dados é
preciso pedir uma área compartilhada ao sistema operacional, e as primitivas de
sincronização precisam ser marcadas como compartilhadas entre processos.
Threads compartilham a memória naturalmente, o que facilita a comunicação e
torna indispensável proteger as alterações simultâneas.

Neste trabalho as duas versões executam o mesmo cálculo, com o mesmo tipo de
barreira e a mesma quantidade de sincronizações. Por isso o desempenho ficou
praticamente igual (veja `analise_resultados.md`). A diferença de custo para
criar processos (`fork`) ou threads aparece uma única vez e é pequena perto dos
segundos de simulação.

## 9. Medição de tempo

`clock_gettime(CLOCK_MONOTONIC, ...)`, que não é afetado por ajustes no relógio
do sistema.

- **Medido:** criação dos trabalhadores, todas as iterações e a espera pelo fim
  (`pthread_join`/`waitpid`).
- **Fora da medição:** alocação, distribuição inicial dos carros e contagem das
  filas no final — iguais nas três versões.

### Por que `__attribute__((noinline))`

Quase todo o tempo é gasto em `atualizar_trecho`. Nas primeiras medições, o
compilador "colava" esse laço dentro de funções diferentes em cada versão
(`inline`) e o mesmo código chegava a ficar 15% mais lento em uma delas. Com 1
trabalhador, processos, threads e sequencial deveriam levar o mesmo tempo, e
não levavam. O atributo mantém o laço como uma função separada nas três
versões, e a comparação passa a medir o paralelismo, não as decisões do
compilador. Com ele, as três versões com 1 trabalhador ficam a menos de 2% uma
da outra.

## 10. Docker e limite de CPU

`docker run --cpus="2"` limita o **tempo de CPU** do container: a cada período
de 100 ms, os processos do container podem usar no total 200 ms de CPU. Eles
podem rodar em qualquer núcleo, mas, esgotada a cota, ficam parados até o
próximo período.

Com mais trabalhadores do que CPUs liberadas, eles rodam juntos em vários
núcleos até gastar a cota e então o grupo inteiro fica parado até o próximo
período. Neste programa isso pesa mais, porque cada iteração tem duas
barreiras: se um trabalhador está parado, todos os outros esperam por ele. Nas
medições, 8 trabalhadores com `--cpus=1` levaram cerca de 3,5 vezes o tempo
da sequencial; o melhor resultado em cada limite veio com o número de
trabalhadores igual ao número de CPUs liberadas (veja
`analise_resultados.md`).

## 11. Partes mais importantes para estudar

1. `atualizar_trecho`: regra de movimento, linha de parada e dois estados;
2. `sortear`: por que o resultado é determinístico;
3. `sinal_verde`, `tentar_entrar_no_cruzamento` e `avancar_cruzamento`;
4. `inicio_do_bloco` e `atualizar_bloco`: divisão das posições entre
   trabalhadores, inclusive quando um bloco cobre mais de uma via;
5. as duas barreiras de cada iteração e por que cada uma é necessária;
6. `mmap`, `sem_init`, barreira compartilhada, `fork` e `waitpid`;
7. `pthread_create`, mutex, barreira e `pthread_join`;
8. `executar_testes.sh`: comparação dos resultados e gravação do CSV.

## 12. Perguntas prováveis

- **Por que não usar só o mutex, sem barreira?** O mutex garante que um trecho
  não rode ao mesmo tempo em dois trabalhadores, mas não garante ordem. A
  barreira garante que todos terminaram uma fase antes de alguém começar a
  próxima.
- **Por que dividir por posições e não só por via?** Com 4 vias, só 4
  trabalhadores teriam trabalho. Dividindo as posições, 2, 4 e 8 trabalhadores
  recebem a mesma quantidade de trabalho; com 4, cada um fica exatamente com
  uma via.
- **O que acontece na fronteira entre dois blocos?** O trabalhador lê (não
  escreve) as posições vizinhas do estado atual. Como ninguém escreve no estado
  atual durante a fase 1, a leitura é segura.
- **Por que a fase 2 é feita por um só trabalhador?** São 4 células; dividir
  esse trabalho custaria mais em sincronização do que economizaria.
- **Por que as vias são tão longas?** Cada iteração custa duas barreiras (cerca
  de 10 a 15 µs cada nesta máquina). Para o paralelismo compensar, cada
  trabalhador precisa ter bastante trabalho entre uma barreira e outra.
