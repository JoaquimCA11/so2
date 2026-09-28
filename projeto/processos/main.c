#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    NUM_DIRECOES = 4,
    NORTE = 0,
    SUL = 1,
    LESTE = 2,
    OESTE = 3,
    DURACAO_VERDE = 10,
    RODADAS_CALCULO = 32,
    MAX_PROCESSOS = 256
};

typedef struct {
    uint64_t filas[NUM_DIRECOES];
    uint64_t carros_gerados;
    uint64_t carros_passaram;
} Simulacao;

/* Tudo nesta estrutura fica na memoria compartilhada criada com mmap. */
typedef struct {
    sem_t mutex_total;
    uint64_t total_gerado;
    unsigned char chegadas[];
} DadosCompartilhados;

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
                                int num_processos) {
    uint64_t tamanho = iteracoes / (uint64_t)num_processos;
    uint64_t sobra = iteracoes % (uint64_t)num_processos;
    uint64_t extras = (uint64_t)id < sobra ? (uint64_t)id : sobra;
    return (uint64_t)id * tamanho + extras;
}

static uint64_t fim_do_bloco(uint64_t iteracoes, int id,
                             int num_processos) {
    return inicio_do_bloco(iteracoes, id + 1, num_processos);
}

static int esperar_semaforo(sem_t *semaforo) {
    while (sem_wait(semaforo) == -1) {
        if (errno != EINTR) {
            return 0;
        }
    }
    return 1;
}

static int trabalho_do_processo(int id, int num_processos,
                                uint64_t iteracoes, uint64_t seed,
                                DadosCompartilhados *dados) {
    uint64_t inicio = inicio_do_bloco(iteracoes, id, num_processos);
    uint64_t fim = fim_do_bloco(iteracoes, id, num_processos);
    uint64_t total_local = 0;
    uint64_t iteracao;

    /* Divisao do trabalho: cada processo calcula um bloco de iteracoes. */
    for (iteracao = inicio; iteracao < fim; iteracao++) {
        int direcao;

        for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
            unsigned char chegou = gerar_carros(seed, iteracao, direcao);
            dados->chegadas[iteracao * NUM_DIRECOES + (uint64_t)direcao] =
                chegou;
            total_local += chegou;
        }
    }

    /* Regiao critica: somente um processo altera o total por vez. */
    if (!esperar_semaforo(&dados->mutex_total)) {
        return 0;
    }
    dados->total_gerado += total_local;
    if (sem_post(&dados->mutex_total) == -1) {
        return 0;
    }

    return 1;
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
                             const DadosCompartilhados *dados,
                             uint64_t iteracoes) {
    uint64_t iteracao;

    simulacao->carros_gerados = dados->total_gerado;

    for (iteracao = 0; iteracao < iteracoes; iteracao++) {
        int direcao;

        for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
            simulacao->filas[direcao] +=
                dados->chegadas[iteracao * NUM_DIRECOES + (uint64_t)direcao];
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
                              uint64_t iteracoes, int num_processos,
                              double tempo) {
    uint64_t esperando = 0;
    int direcao;

    for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
        esperando += simulacao->filas[direcao];
    }

    printf("Simulação finalizada\n\n");
    printf("Iterações: %llu\n", (unsigned long long)iteracoes);
    printf("Número de processos: %d\n", num_processos);
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

static int ler_num_processos(const char *texto, int *num_processos) {
    char *fim;
    long numero = strtol(texto, &fim, 10);

    if (texto[0] == '\0' || *fim != '\0' || numero < 1 ||
        numero > MAX_PROCESSOS) {
        return 0;
    }

    *num_processos = (int)numero;
    return 1;
}

int main(int argc, char *argv[]) {
    uint64_t iteracoes;
    uint64_t seed;
    int num_processos;
    size_t bytes_chegadas;
    size_t tamanho_mapa;
    DadosCompartilhados *dados;
    pid_t *pids;
    int criados = 0;
    int houve_erro = 0;
    int id;
    Simulacao simulacao = {{0, 0, 0, 0}, 0, 0};
    struct timespec inicio;
    struct timespec fim;

    if (argc != 4 || !ler_uint64(argv[1], &iteracoes) ||
        !ler_num_processos(argv[2], &num_processos) ||
        !ler_uint64(argv[3], &seed) || iteracoes == 0) {
        fprintf(stderr, "Uso: %s ITERACOES PROCESSOS SEED\n", argv[0]);
        fprintf(stderr, "ITERACOES deve ser positiva e PROCESSOS deve estar entre 1 e %d.\n",
                MAX_PROCESSOS);
        return EXIT_FAILURE;
    }

    if (iteracoes > (uint64_t)(SIZE_MAX / NUM_DIRECOES) ||
        (size_t)(iteracoes * NUM_DIRECOES) >
            SIZE_MAX - sizeof(DadosCompartilhados)) {
        fprintf(stderr, "Número de iterações grande demais para a memória.\n");
        return EXIT_FAILURE;
    }

    bytes_chegadas = (size_t)(iteracoes * NUM_DIRECOES);
    tamanho_mapa = sizeof(DadosCompartilhados) + bytes_chegadas;

    /* Memoria compartilhada entre o pai e todos os filhos. */
    dados = mmap(NULL, tamanho_mapa, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (dados == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    if (sem_init(&dados->mutex_total, 1, 1) == -1) {
        perror("sem_init");
        munmap(dados, tamanho_mapa);
        return EXIT_FAILURE;
    }
    dados->total_gerado = 0;

    pids = malloc((size_t)num_processos * sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "Não foi possível reservar memória para os processos.\n");
        sem_destroy(&dados->mutex_total);
        munmap(dados, tamanho_mapa);
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (id = 0; id < num_processos; id++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            houve_erro = 1;
            break;
        }
        if (pid == 0) {
            int sucesso = trabalho_do_processo(id, num_processos, iteracoes,
                                               seed, dados);
            _exit(sucesso ? EXIT_SUCCESS : EXIT_FAILURE);
        }

        pids[criados++] = pid;
    }

    /* Sincronizacao: o pai so usa as chegadas depois que todos terminam. */
    for (id = 0; id < criados; id++) {
        int status;

        if (waitpid(pids[id], &status, 0) == -1 || !WIFEXITED(status) ||
            WEXITSTATUS(status) != EXIT_SUCCESS) {
            houve_erro = 1;
        }
    }

    if (criados != num_processos) {
        houve_erro = 1;
    }

    if (!houve_erro) {
        aplicar_chegadas(&simulacao, dados, iteracoes);
        clock_gettime(CLOCK_MONOTONIC, &fim);
        mostrar_resultado(&simulacao, iteracoes, num_processos,
                          diferenca_em_segundos(inicio, fim));
    }

    free(pids);
    sem_destroy(&dados->mutex_total);
    munmap(dados, tamanho_mapa);
    return houve_erro ? EXIT_FAILURE : EXIT_SUCCESS;
}
