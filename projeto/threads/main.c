#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
    NUM_DIRECOES = 4,
    NORTE = 0,
    SUL = 1,
    LESTE = 2,
    OESTE = 3,
    DURACAO_VERDE = 10,
    RODADAS_CALCULO = 32,
    MAX_THREADS = 256
};

typedef struct {
    uint64_t filas[NUM_DIRECOES];
    uint64_t carros_gerados;
    uint64_t carros_passaram;
} Simulacao;

typedef struct {
    int id;
    int num_threads;
    uint64_t iteracoes;
    uint64_t seed;
    unsigned char *chegadas;
    uint64_t *total_gerado;
    pthread_mutex_t *mutex_total;
} ArgumentosThread;

static unsigned char gerar_carros(uint64_t seed, uint64_t iteracao,
                                  int direcao) {
    uint64_t valor;
    int rodada;

    valor = seed;
    valor ^= (iteracao + 1) * 0x9E3779B97F4A7C15ULL;
    valor ^= (uint64_t)(direcao + 1) * 0xBF58476D1CE4E5B9ULL;

    for (rodada = 0; rodada < RODADAS_CALCULO; rodada++) {
        valor ^= valor >> 12;
        valor ^= valor << 25;
        valor ^= valor >> 27;
        valor *= 0x2545F4914F6CDD1DULL;
        valor += (uint64_t)rodada + 0x94D049BB133111EBULL;
    }

    return (unsigned char)((valor % 100) < 25);
}

static uint64_t inicio_do_bloco(uint64_t iteracoes, int id,
                                int num_threads) {
    uint64_t tamanho = iteracoes / (uint64_t)num_threads;
    uint64_t sobra = iteracoes % (uint64_t)num_threads;
    uint64_t extras = (uint64_t)id < sobra ? (uint64_t)id : sobra;
    return (uint64_t)id * tamanho + extras;
}

static uint64_t fim_do_bloco(uint64_t iteracoes, int id, int num_threads) {
    return inicio_do_bloco(iteracoes, id + 1, num_threads);
}

static void *trabalho_da_thread(void *argumento) {
    ArgumentosThread *args = (ArgumentosThread *)argumento;
    uint64_t inicio = inicio_do_bloco(args->iteracoes, args->id,
                                      args->num_threads);
    uint64_t fim = fim_do_bloco(args->iteracoes, args->id,
                                args->num_threads);
    uint64_t total_local = 0;
    uint64_t iteracao;

    /* Divisao do trabalho: cada thread calcula um bloco de iteracoes. */
    for (iteracao = inicio; iteracao < fim; iteracao++) {
        int direcao;

        for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
            unsigned char chegou = gerar_carros(args->seed, iteracao, direcao);
            args->chegadas[iteracao * NUM_DIRECOES + (uint64_t)direcao] =
                chegou;
            total_local += chegou;
        }
    }

    /* Regiao critica: somente uma thread altera o total por vez. */
    pthread_mutex_lock(args->mutex_total);
    *args->total_gerado += total_local;
    pthread_mutex_unlock(args->mutex_total);
    return NULL;
}

static void processar_direcao(Simulacao *simulacao, int direcao) {
    if (simulacao->filas[direcao] > 0) {
        simulacao->filas[direcao]--;
        simulacao->carros_passaram++;
    }
}

static void atualizar_semaforo_e_passar(Simulacao *simulacao,
                                         uint64_t iteracao) {
    uint64_t fase = (iteracao / DURACAO_VERDE) % 2;

    if (fase == 0) {
        processar_direcao(simulacao, NORTE);
        processar_direcao(simulacao, SUL);
    } else {
        processar_direcao(simulacao, LESTE);
        processar_direcao(simulacao, OESTE);
    }
}

static void aplicar_chegadas(Simulacao *simulacao,
                             const unsigned char *chegadas,
                             uint64_t total_gerado, uint64_t iteracoes) {
    uint64_t iteracao;

    simulacao->carros_gerados = total_gerado;

    for (iteracao = 0; iteracao < iteracoes; iteracao++) {
        int direcao;

        for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
            simulacao->filas[direcao] +=
                chegadas[iteracao * NUM_DIRECOES + (uint64_t)direcao];
        }
        atualizar_semaforo_e_passar(simulacao, iteracao);
    }
}

static double diferenca_em_segundos(struct timespec inicio,
                                    struct timespec fim) {
    double segundos = (double)(fim.tv_sec - inicio.tv_sec);
    double nanossegundos = (double)(fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
    return segundos + nanossegundos;
}

static void mostrar_resultado(const Simulacao *simulacao,
                              uint64_t iteracoes, int num_threads,
                              double tempo) {
    uint64_t esperando = 0;
    int direcao;

    for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
        esperando += simulacao->filas[direcao];
    }

    printf("Simulação finalizada\n\n");
    printf("Iterações: %llu\n", (unsigned long long)iteracoes);
    printf("Número de threads: %d\n", num_threads);
    printf("Carros gerados: %llu\n",
           (unsigned long long)simulacao->carros_gerados);
    printf("Carros que passaram: %llu\n",
           (unsigned long long)simulacao->carros_passaram);
    printf("Carros esperando: %llu\n\n", (unsigned long long)esperando);
    printf("Fila Norte: %llu\n", (unsigned long long)simulacao->filas[NORTE]);
    printf("Fila Sul: %llu\n", (unsigned long long)simulacao->filas[SUL]);
    printf("Fila Leste: %llu\n", (unsigned long long)simulacao->filas[LESTE]);
    printf("Fila Oeste: %llu\n\n", (unsigned long long)simulacao->filas[OESTE]);
    printf("Tempo de execução: %.6f segundos\n", tempo);
}

static int ler_uint64(const char *texto, uint64_t *valor) {
    char *fim;
    unsigned long long numero;

    if (texto[0] == '-') {
        return 0;
    }

    numero = strtoull(texto, &fim, 10);
    if (texto[0] == '\0' || *fim != '\0') {
        return 0;
    }

    *valor = (uint64_t)numero;
    return 1;
}

static int ler_num_threads(const char *texto, int *num_threads) {
    char *fim;
    long numero = strtol(texto, &fim, 10);

    if (texto[0] == '\0' || *fim != '\0' || numero < 1 ||
        numero > MAX_THREADS) {
        return 0;
    }

    *num_threads = (int)numero;
    return 1;
}

int main(int argc, char *argv[]) {
    uint64_t iteracoes;
    uint64_t seed;
    int num_threads;
    size_t bytes_chegadas;
    unsigned char *chegadas;
    pthread_t *threads;
    ArgumentosThread *argumentos;
    pthread_mutex_t mutex_total;
    uint64_t total_gerado = 0;
    int criadas = 0;
    int houve_erro = 0;
    int id;
    Simulacao simulacao = {{0, 0, 0, 0}, 0, 0};
    struct timespec inicio;
    struct timespec fim;

    if (argc != 4 || !ler_uint64(argv[1], &iteracoes) ||
        !ler_num_threads(argv[2], &num_threads) ||
        !ler_uint64(argv[3], &seed) || iteracoes == 0) {
        fprintf(stderr, "Uso: %s ITERACOES THREADS SEED\n", argv[0]);
        fprintf(stderr, "ITERACOES deve ser positiva e THREADS deve estar entre 1 e %d.\n",
                MAX_THREADS);
        return EXIT_FAILURE;
    }

    if (iteracoes > (uint64_t)(SIZE_MAX / NUM_DIRECOES)) {
        fprintf(stderr, "Número de iterações grande demais para a memória.\n");
        return EXIT_FAILURE;
    }

    bytes_chegadas = (size_t)(iteracoes * NUM_DIRECOES);
    chegadas = malloc(bytes_chegadas);
    threads = malloc((size_t)num_threads * sizeof(pthread_t));
    argumentos = malloc((size_t)num_threads * sizeof(ArgumentosThread));

    if (chegadas == NULL || threads == NULL || argumentos == NULL) {
        fprintf(stderr, "Não foi possível reservar memória.\n");
        free(chegadas);
        free(threads);
        free(argumentos);
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&mutex_total, NULL) != 0) {
        fprintf(stderr, "Não foi possível criar o mutex.\n");
        free(chegadas);
        free(threads);
        free(argumentos);
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (id = 0; id < num_threads; id++) {
        argumentos[id].id = id;
        argumentos[id].num_threads = num_threads;
        argumentos[id].iteracoes = iteracoes;
        argumentos[id].seed = seed;
        argumentos[id].chegadas = chegadas;
        argumentos[id].total_gerado = &total_gerado;
        argumentos[id].mutex_total = &mutex_total;

        if (pthread_create(&threads[id], NULL, trabalho_da_thread,
                           &argumentos[id]) != 0) {
            fprintf(stderr, "Não foi possível criar a thread %d.\n", id);
            houve_erro = 1;
            break;
        }
        criadas++;
    }

    /* Sincronizacao: a principal so usa as chegadas depois dos joins. */
    for (id = 0; id < criadas; id++) {
        if (pthread_join(threads[id], NULL) != 0) {
            houve_erro = 1;
        }
    }

    if (criadas != num_threads) {
        houve_erro = 1;
    }

    if (!houve_erro) {
        aplicar_chegadas(&simulacao, chegadas, total_gerado, iteracoes);
        clock_gettime(CLOCK_MONOTONIC, &fim);
        mostrar_resultado(&simulacao, iteracoes, num_threads,
                          diferenca_em_segundos(inicio, fim));
    }

    pthread_mutex_destroy(&mutex_total);
    free(chegadas);
    free(threads);
    free(argumentos);
    return houve_erro ? EXIT_FAILURE : EXIT_SUCCESS;
}
