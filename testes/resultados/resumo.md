# Resumo dos resultados

Gerado por `testes/gerar_graficos.py` a partir dos CSVs desta pasta.

Entrada: CARROS=100000, TAMANHO_VIA=100000, TEMPO_SEMAFORO=30, ITERACOES=3000, SEED=123

## Fora do container

Speedup = tempo médio sequencial / tempo médio paralelo. Eficiência = speedup / trabalhadores.

| Versão | Trabalhadores | Execuções | Tempo médio (s) | Desvio padrão (s) | Speedup | Eficiência |
|---|---|---|---|---|---|---|
| Sequencial | 1 | 3 | 4,292 | 0,018 | 1,00 | 100% |
| Processos | 1 | 3 | 4,345 | 0,018 | 0,99 | 99% |
| Processos | 2 | 3 | 2,776 | 0,104 | 1,55 | 77% |
| Processos | 4 | 3 | 1,704 | 0,010 | 2,52 | 63% |
| Processos | 8 | 3 | 1,372 | 0,184 | 3,13 | 39% |
| Threads | 1 | 3 | 4,372 | 0,013 | 0,98 | 98% |
| Threads | 2 | 3 | 2,781 | 0,115 | 1,54 | 77% |
| Threads | 4 | 3 | 1,722 | 0,010 | 2,49 | 62% |
| Threads | 8 | 3 | 1,363 | 0,089 | 3,15 | 39% |

## Dentro do container

Speedup no container = sequencial no mesmo limite de CPU / paralelo. Dentro/fora = tempo no container / tempo fora do container.

| Versão | CPUs | Trabalhadores | Execuções | Tempo médio (s) | Desvio padrão (s) | Speedup no container | Dentro/fora |
|---|---|---|---|---|---|---|---|
| Sequencial | 1 | 1 | 3 | 4,423 | 0,073 | 1,00 | 1,03 |
| Sequencial | 2 | 1 | 3 | 4,641 | 0,041 | 1,00 | 1,08 |
| Sequencial | 4 | 1 | 3 | 4,498 | 0,016 | 1,00 | 1,05 |
| Processos | 1 | 2 | 3 | 6,076 | 0,048 | 0,73 | 2,19 |
| Processos | 1 | 4 | 3 | 11,731 | 0,108 | 0,38 | 6,89 |
| Processos | 1 | 8 | 3 | 16,226 | 0,109 | 0,27 | 11,83 |
| Processos | 2 | 2 | 3 | 3,077 | 0,063 | 1,51 | 1,11 |
| Processos | 2 | 4 | 3 | 4,152 | 0,049 | 1,12 | 2,44 |
| Processos | 2 | 8 | 3 | 5,729 | 0,240 | 0,81 | 4,17 |
| Processos | 4 | 2 | 3 | 3,078 | 0,016 | 1,46 | 1,11 |
| Processos | 4 | 4 | 3 | 1,784 | 0,020 | 2,52 | 1,05 |
| Processos | 4 | 8 | 3 | 2,495 | 0,016 | 1,80 | 1,82 |
| Threads | 1 | 2 | 3 | 6,183 | 0,083 | 0,72 | 2,22 |
| Threads | 1 | 4 | 3 | 11,532 | 0,065 | 0,38 | 6,70 |
| Threads | 1 | 8 | 3 | 15,416 | 1,143 | 0,29 | 11,31 |
| Threads | 2 | 2 | 3 | 3,201 | 0,118 | 1,45 | 1,15 |
| Threads | 2 | 4 | 3 | 3,975 | 0,114 | 1,17 | 2,31 |
| Threads | 2 | 8 | 3 | 6,171 | 0,185 | 0,75 | 4,53 |
| Threads | 4 | 2 | 3 | 2,893 | 0,026 | 1,55 | 1,04 |
| Threads | 4 | 4 | 3 | 1,723 | 0,016 | 2,61 | 1,00 |
| Threads | 4 | 8 | 3 | 2,418 | 0,070 | 1,86 | 1,77 |
