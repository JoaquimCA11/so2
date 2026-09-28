#define _POSIX_C_SOURCE 200809L

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
    RODADAS_CALCULO = 32
};

typedef struct {
    uint64_t filas[NUM_DIRECOES];
    uint64_t carros_gerados;
    uint64_t carros_passaram;
} Simulacao;

/*
 * Gera sempre o mesmo resultado para a mesma seed, iteracao e direcao.
 * As rodadas representam o trabalho de calcular um evento de chegada.
 */
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

static double diferenca_em_segundos(struct timespec inicio,
                                    struct timespec fim) {
    double segundos = (double)(fim.tv_sec - inicio.tv_sec);
    double nanossegundos = (double)(fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
    return segundos + nanossegundos;
}

static void mostrar_resultado(const Simulacao *simulacao,
                              uint64_t iteracoes, double tempo) {
    uint64_t esperando = 0;
    int direcao;

    for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
        esperando += simulacao->filas[direcao];
    }

    printf("Simulação finalizada\n\n");
    printf("Iterações: %llu\n", (unsigned long long)iteracoes);
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

int main(int argc, char *argv[]) {
    uint64_t iteracoes;
    uint64_t seed;
    uint64_t iteracao;
    Simulacao simulacao = {{0, 0, 0, 0}, 0, 0};
    struct timespec inicio;
    struct timespec fim;

    if (argc != 3 || !ler_uint64(argv[1], &iteracoes) ||
        !ler_uint64(argv[2], &seed) || iteracoes == 0) {
        fprintf(stderr, "Uso: %s ITERACOES SEED\n", argv[0]);
        fprintf(stderr, "ITERACOES deve ser maior que zero.\n");
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (iteracao = 0; iteracao < iteracoes; iteracao++) {
        int direcao;

        for (direcao = 0; direcao < NUM_DIRECOES; direcao++) {
            unsigned char chegou = gerar_carros(seed, iteracao, direcao);
            simulacao.filas[direcao] += chegou;
            simulacao.carros_gerados += chegou;
        }

        atualizar_semaforo_e_passar(&simulacao, iteracao);
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    mostrar_resultado(&simulacao, iteracoes,
                      diferenca_em_segundos(inicio, fim));
    return EXIT_SUCCESS;
}
