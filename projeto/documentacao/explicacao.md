# Explicação do projeto

## 1. O que o programa simula

O programa representa um cruzamento com uma fila para cada direção: Norte,
Sul, Leste e Oeste. Há somente uma faixa por direção. Os carros chegam às
filas e só podem atravessar quando o semáforo de sua direção está verde.

Uma **iteração** é uma etapa da simulação. Em cada iteração acontecem, nesta
ordem:

1. calcula-se se chegou um carro em cada uma das quatro direções;
2. os carros que chegaram são colocados nas filas;
3. descobre-se qual par de direções está com sinal verde;
4. no máximo um carro de cada direção verde atravessa.

O sinal fica verde por 10 iterações para Norte/Sul e depois por 10 iterações
para Leste/Oeste. Esse ciclo se repete até o fim.

## 2. Como os carros são representados

Não é necessário guardar uma estrutura para cada carro. Como os carros de uma
mesma faixa são equivalentes para esta simulação, basta um contador por fila.
Quando um carro chega, o contador aumenta. Quando ele passa, o contador diminui
e `carros_passaram` aumenta.

Os dados principais são:

- `filas[4]`: quantidade esperando em Norte, Sul, Leste e Oeste;
- `carros_gerados`: total de carros que chegaram;
- `carros_passaram`: total que atravessou;
- número da iteração, usado para determinar a fase do semáforo.

Ao final sempre deve valer:

```text
carros gerados = carros que passaram + carros esperando
```

## 3. Geração determinística

A função `gerar_carros` usa somente três entradas: seed, número da iteração e
direção. Ela faz uma sequência fixa de operações com inteiros sem sinal e
retorna 0 ou 1. O resultado 1 significa que um carro chegou.

Essa função não depende da ordem em que os trabalhadores são executados. Por
isso, a chegada da iteração 500 no lado Norte será idêntica com 2, 4 ou 8
trabalhadores. As operações repetidas na função também fornecem trabalho
computacional real, sem `sleep`. Para aumentar a duração do teste, basta
aumentar o número de iterações.

## 4. Versão sequencial

A versão sequencial usa um único fluxo de execução. O laço principal percorre
as iterações em ordem. Em cada uma, ele chama `gerar_carros` quatro vezes,
atualiza as filas e chama `atualizar_semaforo_e_passar`.

Essa versão é a referência: processos e threads devem chegar aos mesmos
contadores finais.

## 5. Por que a simulação paralela tem duas fases

O estado de uma fila na iteração seguinte depende do estado atual. Aplicar as
iterações fora de ordem poderia mudar o resultado. Para manter a solução fácil
de explicar e correta, as versões paralelas foram divididas assim:

1. **fase paralela:** os trabalhadores calculam as chegadas de blocos
   diferentes de iterações;
2. **fase ordenada:** depois que todos terminam, o coordenador percorre a tabela
   de chegadas em ordem e atualiza filas e semáforo.

Essa é a adaptação usada para permitir 2, 4 e 8 trabalhadores. A parte de maior
custo, que é o cálculo dos eventos de chegada, é dividida. A atualização do
estado continua ordenada para preservar o significado da simulação.

A tabela usa um byte para cada direção em cada iteração, ou seja, cerca de
quatro bytes por iteração. Esse custo de memória é uma consequência simples da
separação entre a fase paralela e a fase ordenada.

Se existem `N` iterações e `P` trabalhadores, cada trabalhador recebe
aproximadamente `N / P` iterações. Quando a divisão tem resto, os primeiros
trabalhadores recebem uma iteração extra.

## 6. Versão com processos

`fork()` cria processos filhos. Processos normalmente têm espaços de memória
separados, então uma alteração feita por um filho não apareceria para o pai.
Por isso, o programa chama `mmap()` com `MAP_SHARED | MAP_ANONYMOUS`.

A área compartilhada guarda:

- a tabela `chegadas`, com quatro posições por iteração;
- `total_gerado`, atualizado com a soma produzida por cada filho;
- `mutex_total`, que neste caso é um semáforo POSIX usado como mutex.

Cada filho escreve somente no bloco da tabela que recebeu. Depois, ele precisa
somar seu resultado local em `total_gerado`, que é compartilhado. O trecho
entre `sem_wait` e `sem_post` é a **região crítica**. O semáforo começa com valor
1: um processo entra, os demais esperam e, ao sair, ele libera o próximo. Isso
evita que duas somas simultâneas sobrescrevam uma à outra.

O pai usa `waitpid()` para esperar todos os filhos. Essa espera é outra
sincronização importante: sem ela, o pai poderia ler posições que ainda não
foram calculadas. A tabela é a comunicação entre filhos e pai; os filhos
produzem os eventos e o pai obrigatoriamente os consome.

## 7. Versão com threads

Threads do mesmo processo já compartilham variáveis e memória alocada com
`malloc`. Todas recebem o endereço da mesma tabela `chegadas` e do mesmo
`total_gerado`.

`pthread_create()` inicia cada trabalhadora. Cada thread calcula seu bloco e
escreve em posições diferentes da tabela. A soma em `total_gerado` é uma região
crítica protegida por `pthread_mutex_lock()` e `pthread_mutex_unlock()`.

A thread principal chama `pthread_join()` para esperar cada trabalhadora. Só
depois dos joins ela aplica as chegadas às filas. Uma barreira não foi usada
porque há apenas um encontro entre as duas fases; nesse caso, `pthread_join()`
é mais simples.

## 8. Sincronização e região crítica

Uma região crítica é um trecho que acessa um dado compartilhado que não pode
ser alterado por dois trabalhadores ao mesmo tempo. Imagine dois trabalhadores
lendo o total 100, somando 5 e gravando 105: uma das somas seria perdida.

Na versão com processos, `sem_wait`/`sem_post` protegem esse trecho. Na versão
com threads, `pthread_mutex_lock`/`pthread_mutex_unlock` fazem o mesmo papel.
`waitpid` e `pthread_join` também sincronizam as fases de produzir e consumir.

## 9. Processos e threads

Um processo tem seu próprio espaço de endereçamento e costuma ter maior custo
de criação. Para compartilhar dados, foi necessário pedir explicitamente uma
área compartilhada ao sistema operacional.

Uma thread é um fluxo dentro do mesmo processo. Threads compartilham a memória
naturalmente e costumam ser mais leves. Esse compartilhamento facilita a
comunicação, mas também torna indispensável proteger alterações simultâneas.

## 10. Como os resultados permanecem equivalentes

Quatro decisões garantem a equivalência:

1. a função de geração é exatamente a mesma nas três versões;
2. cada evento depende de seed, iteração e direção, nunca da ordem de execução;
3. cada posição da tabela corresponde a uma iteração e direção específicas;
4. filas e semáforo são atualizados na mesma ordem da versão sequencial.

O script de testes extrai sete valores de cada saída — total gerado, total que
passou, total esperando e as quatro filas — e interrompe com erro se algum
deles for diferente da referência sequencial.

## 11. Medição e speedup

As versões usam `clock_gettime(CLOCK_MONOTONIC, ...)`. Esse relógio é apropriado
para duração porque não é afetado por ajustes no horário do sistema.

O speedup indica quantas vezes a versão paralela foi mais rápida:

```text
speedup = tempo sequencial / tempo paralelo
```

Se a versão sequencial levou 6 segundos e a versão com 4 threads levou 2:

```text
speedup = 6 / 2 = 3
```

Os números acima apenas demonstram a fórmula. Os valores do trabalho devem vir
do arquivo `testes/resultados.csv`, preenchido por execuções reais.

O speedup não cresce de forma perfeita. Criar processos ou threads custa
tempo, a fase de atualizar as filas continua sequencial, a região crítica pode
causar espera e os trabalhadores disputam memória e cache. Assim, 8
trabalhadores nem sempre serão duas vezes mais rápidos que 4. Em algumas
máquinas, podem até ser mais lentos.

## 12. Docker e limite de CPU

A opção `--cpus` limita quanto processamento o contêiner pode usar. O programa
pode criar 8 threads dentro de um contêiner limitado a 2 CPUs, mas somente uma
quantidade equivalente a 2 CPUs poderá executar ao mesmo tempo. Os outros
trabalhadores terão de revezar, aumentando trocas de contexto e reduzindo o
speedup.

Para uma comparação justa, registre o limite usado. Testar 2 trabalhadores com
2 CPUs, 4 com 4 CPUs e 8 com 8 CPUs mostra a capacidade disponível. Também é
útil manter o mesmo limite para todas as versões quando a pergunta do
experimento for comparar apenas a estratégia.

## 13. Partes mais importantes para estudar

Antes da apresentação, acompanhe no código:

1. `gerar_carros`: por que o resultado é determinístico;
2. `atualizar_semaforo_e_passar`: alternância do verde e retirada das filas;
3. `inicio_do_bloco` e `fim_do_bloco`: divisão equilibrada das iterações;
4. `mmap`, `fork`, `sem_wait`, `sem_post` e `waitpid` na versão com processos;
5. `pthread_create`, mutex e `pthread_join` na versão com threads;
6. `aplicar_chegadas`: parte ordenada comum ao modelo paralelo;
7. `clock_gettime`: limites da medição;
8. `executar_testes.sh`: comparação dos resultados e gravação do CSV.

Uma pergunta provável é por que não paralelizar diretamente a alteração das
filas. A resposta é que iterações consecutivas dependem uma da outra. O projeto
paraleliza o cálculo independente dos eventos e preserva a aplicação ordenada,
o que mantém o resultado determinístico e o código compreensível.
