#define _POSIX_C_SOURCE 200809L
/* MAP_ANONYMOUS não faz parte do POSIX 2008; no glibc ele vem com esta macro. */
#define _DEFAULT_SOURCE

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    NUM_VIAS = 4,
    NORTE = 0,
    SUL = 1,
    LESTE = 2,
    OESTE = 3,
    CHANCE_HESITAR = 10, /* % de chance de um motorista não avançar */
    MAX_PROCESSOS = 256
};

/*
 * O cruzamento tem 2x2 células e o trânsito anda pela mão direita. Cada via
 * atravessa por duas células: entra na primeira e depois passa para a segunda.
 * Vias do mesmo eixo (Norte/Sul ou Leste/Oeste) não usam células em comum.
 */
enum {
    NOROESTE = 0,
    NORDESTE = 1,
    SUDOESTE = 2,
    SUDESTE = 3,
    CELULAS_CRUZAMENTO = 4
};

static const int PRIMEIRA_CELULA[NUM_VIAS] = {NOROESTE, SUDESTE, NORDESTE,
                                              SUDOESTE};
static const int SEGUNDA_CELULA[NUM_VIAS] = {SUDOESTE, NORDESTE, NOROESTE,
                                             SUDESTE};
static const char *const NOME_VIA[NUM_VIAS] = {"Norte", "Sul", "Leste",
                                               "Oeste"};

typedef struct {
    uint64_t carros;
    uint64_t tamanho_via;
    uint64_t tempo_semaforo;
    uint64_t iteracoes;
    uint64_t seed;
} Parametros;

/* O semáforo (usado como mutex) fica junto do recurso que ele protege. */
typedef struct {
    sem_t mutex;
    unsigned char celulas[CELULAS_CRUZAMENTO]; /* 0 = livre, via + 1 = ocupada */
    uint64_t carros_no_cruzamento;
    uint64_t carros_atravessaram;
} Cruzamento;

/*
 * Tudo o que muda durante a simulação fica nesta área criada com mmap e
 * compartilhada entre o pai e os filhos. Os parâmetros não mudam, então cada
 * filho usa a cópia que recebeu no fork.
 */
typedef struct {
    pthread_barrier_t barreira;
    Cruzamento cruzamento;
    unsigned char celulas[]; /* 2 estados x 4 vias x tamanho_via */
} DadosCompartilhados;

/*
 * Número pseudoaleatório que depende somente dos argumentos. Assim o sorteio
 * de um carro não depende de qual trabalhador o calcula nem da ordem.
 */
static uint64_t sortear(uint64_t seed, uint64_t iteracao, uint64_t via,
                        uint64_t posicao) {
    uint64_t valor = seed;

    valor ^= (iteracao + 1) * 0x9E3779B97F4A7C15ULL;
    valor ^= (via + 1) * 0xC2B2AE3D27D4EB4FULL;
    valor ^= (posicao + 1) * 0x165667B19E3779F9ULL;
    valor ^= valor >> 30;
    valor *= 0xBF58476D1CE4E5B9ULL;
    valor ^= valor >> 27;
    valor *= 0x94D049BB133111EBULL;
    valor ^= valor >> 31;
    return valor;
}

/*
 * Coloca exatamente carros / 4 carros em cada via, em posições sorteadas.
 * O sorteio da posição inicial usa via + NUM_VIAS para não coincidir com o
 * sorteio de hesitação.
 */
static void distribuir_carros(const Parametros *p, unsigned char *celulas) {
    int via;

    for (via = 0; via < NUM_VIAS; via++) {
        unsigned char *trecho = celulas + (uint64_t)via * p->tamanho_via;
        uint64_t restantes = p->carros / NUM_VIAS +
                             ((uint64_t)via < p->carros % NUM_VIAS ? 1 : 0);
        uint64_t posicao;

        for (posicao = 0; posicao < p->tamanho_via; posicao++) {
            uint64_t livres = p->tamanho_via - posicao;
            uint64_t sorteio = sortear(p->seed, 0, (uint64_t)via + NUM_VIAS,
                                       posicao);
            unsigned char recebe = (unsigned char)(sorteio % livres < restantes);

            trecho[posicao] = recebe;
            restantes -= recebe;
        }
    }
}

static int sinal_verde(const Parametros *p, int via, uint64_t iteracao) {
    int verde_norte_sul = (iteracao / p->tempo_semaforo) % 2 == 0;
    int via_norte_sul = via == NORTE || via == SUL;
    return verde_norte_sul == via_norte_sul;
}

static void esperar_semaforo(sem_t *semaforo) {
    while (sem_wait(semaforo) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            _exit(EXIT_FAILURE);
        }
    }
}

/*
 * Região crítica: dois processos (por exemplo, os donos das vias Norte e Sul)
 * podem tentar entrar no mesmo passo. Verificar se a célula está livre,
 * ocupá-la e atualizar o contador precisa acontecer de uma vez só.
 * noinline: a parte que muda entre as versões fica fora do laço principal.
 */
__attribute__((noinline)) static int tentar_entrar_no_cruzamento(Cruzamento *cruzamento, int via) {
    int celula = PRIMEIRA_CELULA[via];
    int entrou = 0;

    esperar_semaforo(&cruzamento->mutex);
    if (cruzamento->celulas[celula] == 0) {
        cruzamento->celulas[celula] = (unsigned char)(via + 1);
        cruzamento->carros_no_cruzamento++;
        entrou = 1;
    }
    sem_post(&cruzamento->mutex);
    return entrou;
}

/* O carro anda se a célula da frente estiver livre e o motorista não hesitar. */
static int carro_avanca(const Parametros *p, const unsigned char *trecho,
                        int via, uint64_t posicao, uint64_t iteracao) {
    return trecho[posicao] && !trecho[posicao + 1] &&
           sortear(p->seed, iteracao, (uint64_t)via, posicao) % 100 >=
               CHANCE_HESITAR;
}

/*
 * Calcula as posições [inicio, fim) de uma via na próxima iteração. Lê somente
 * o estado atual e escreve somente nas próprias posições do estado seguinte.
 * A última posição é a linha de parada, de onde o carro entra no cruzamento.
 *
 * noinline (extensão do GCC) mantém este laço, que consome quase todo o
 * tempo, como uma função separada nas três versões. Sem isso o compilador o
 * encaixa de formas diferentes em cada programa e o mesmo laço chega a variar
 * 15% de tempo, o que distorceria a comparação entre as versões.
 */
__attribute__((noinline)) static void atualizar_trecho(const Parametros *p, Cruzamento *cruzamento,
                             const unsigned char *atual,
                             unsigned char *proxima, int via, uint64_t inicio,
                             uint64_t fim, uint64_t iteracao) {
    uint64_t ultima = p->tamanho_via - 1;
    int chega = inicio > 0 && carro_avanca(p, atual, via, inicio - 1, iteracao);
    uint64_t posicao;

    for (posicao = inicio; posicao < fim; posicao++) {
        int sai = 0;

        if (atual[posicao]) {
            if (posicao < ultima) {
                sai = carro_avanca(p, atual, via, posicao, iteracao);
            } else if (sinal_verde(p, via, iteracao)) {
                sai = tentar_entrar_no_cruzamento(cruzamento, via);
            }
        }

        proxima[posicao] = (unsigned char)((atual[posicao] && !sai) || chega);
        chega = sai;
    }
}

/*
 * As quatro vias ficam em sequência na memória. Um bloco [inicio, fim) pode
 * cobrir uma via inteira, parte dela ou várias vias.
 */
static void atualizar_bloco(const Parametros *p, Cruzamento *cruzamento,
                            unsigned char *celulas, uint64_t inicio,
                            uint64_t fim, uint64_t iteracao) {
    uint64_t total = NUM_VIAS * p->tamanho_via;
    const unsigned char *atual = celulas + (iteracao % 2) * total;
    unsigned char *proxima = celulas + ((iteracao + 1) % 2) * total;
    int via;

    for (via = 0; via < NUM_VIAS; via++) {
        uint64_t comeco_via = (uint64_t)via * p->tamanho_via;
        uint64_t fim_via = comeco_via + p->tamanho_via;
        uint64_t de = inicio > comeco_via ? inicio : comeco_via;
        uint64_t ate = fim < fim_via ? fim : fim_via;

        if (de < ate) {
            atualizar_trecho(p, cruzamento, atual + comeco_via,
                             proxima + comeco_via, via, de - comeco_via,
                             ate - comeco_via, iteracao);
        }
    }
}

/* Carros na segunda célula saem; carros na primeira passam para a segunda. */
static void avancar_cruzamento(Cruzamento *cruzamento) {
    unsigned char novas[CELULAS_CRUZAMENTO] = {0, 0, 0, 0};
    int celula;

    for (celula = 0; celula < CELULAS_CRUZAMENTO; celula++) {
        int ocupante = cruzamento->celulas[celula];

        if (ocupante == 0) {
            continue;
        }
        if (celula == SEGUNDA_CELULA[ocupante - 1]) {
            cruzamento->carros_no_cruzamento--;
            cruzamento->carros_atravessaram++;
        } else {
            novas[SEGUNDA_CELULA[ocupante - 1]] = (unsigned char)ocupante;
        }
    }

    for (celula = 0; celula < CELULAS_CRUZAMENTO; celula++) {
        cruzamento->celulas[celula] = novas[celula];
    }
}

static uint64_t inicio_do_bloco(uint64_t total, int id, int num_processos) {
    uint64_t tamanho = total / (uint64_t)num_processos;
    uint64_t sobra = total % (uint64_t)num_processos;
    uint64_t extras = (uint64_t)id < sobra ? (uint64_t)id : sobra;
    return (uint64_t)id * tamanho + extras;
}

static void trabalho_do_processo(int id, int num_processos,
                                 const Parametros *p,
                                 DadosCompartilhados *dados) {
    uint64_t total = NUM_VIAS * p->tamanho_via;
    uint64_t inicio = inicio_do_bloco(total, id, num_processos);
    uint64_t fim = inicio_do_bloco(total, id + 1, num_processos);
    uint64_t iteracao;

    for (iteracao = 0; iteracao < p->iteracoes; iteracao++) {
        /* Fase 1 (paralela): cada processo move os carros do seu bloco. */
        atualizar_bloco(p, &dados->cruzamento, dados->celulas, inicio, fim,
                        iteracao);

        /* Todos terminam a fase 1 antes de o cruzamento andar. */
        pthread_barrier_wait(&dados->barreira);

        /* Fase 2 (um processo): os carros dentro do cruzamento andam. */
        if (id == 0) {
            avancar_cruzamento(&dados->cruzamento);
        }

        /* Ninguém começa a próxima iteração com o cruzamento desatualizado. */
        pthread_barrier_wait(&dados->barreira);
    }
}

static double diferenca_em_segundos(struct timespec inicio,
                                    struct timespec fim) {
    double segundos = (double)(fim.tv_sec - inicio.tv_sec);
    double nanossegundos = (double)(fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
    return segundos + nanossegundos;
}

/* A fila é formada pelos carros encostados atrás da linha de parada. */
static void mostrar_resultado(const Parametros *p,
                              const Cruzamento *cruzamento,
                              const unsigned char *celulas, int num_processos,
                              double tempo) {
    uint64_t filas[NUM_VIAS] = {0, 0, 0, 0};
    uint64_t nas_vias = 0;
    int via;

    for (via = 0; via < NUM_VIAS; via++) {
        const unsigned char *trecho = celulas + (uint64_t)via * p->tamanho_via;
        uint64_t posicao;

        for (posicao = 0; posicao < p->tamanho_via; posicao++) {
            nas_vias += trecho[posicao];
        }
        for (posicao = p->tamanho_via; posicao > 0 && trecho[posicao - 1];
             posicao--) {
            filas[via]++;
        }
    }

    printf("Simulação finalizada\n\n");
    printf("Carros: %llu\n", (unsigned long long)p->carros);
    printf("Tamanho das vias: %llu\n", (unsigned long long)p->tamanho_via);
    printf("Tempo do semáforo: %llu\n", (unsigned long long)p->tempo_semaforo);
    printf("Iterações: %llu\n", (unsigned long long)p->iteracoes);
    printf("Número de processos: %d\n\n", num_processos);
    printf("Carros que atravessaram: %llu\n",
           (unsigned long long)cruzamento->carros_atravessaram);
    printf("Carros no cruzamento: %llu\n",
           (unsigned long long)cruzamento->carros_no_cruzamento);
    printf("Carros nas vias: %llu\n\n", (unsigned long long)nas_vias);
    for (via = 0; via < NUM_VIAS; via++) {
        printf("Fila %s: %llu\n", NOME_VIA[via], (unsigned long long)filas[via]);
    }
    printf("\nTempo de execução: %.6f segundos\n", tempo);
}

static int ler_uint64(const char *texto, uint64_t *valor) {
    char *fim;
    unsigned long long numero;

    if (texto[0] == '\0' || texto[0] == '-') {
        return 0;
    }

    errno = 0;
    numero = strtoull(texto, &fim, 10);
    if (errno != 0 || *fim != '\0') {
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

static int validar_parametros(const Parametros *p) {
    if (p->tamanho_via == 0 || p->tempo_semaforo == 0 || p->iteracoes == 0) {
        fprintf(stderr, "TAMANHO_VIA, TEMPO_SEMAFORO e ITERACOES devem ser "
                        "maiores que zero.\n");
        return 0;
    }
    if (p->tamanho_via > SIZE_MAX / (2 * NUM_VIAS)) {
        fprintf(stderr, "TAMANHO_VIA grande demais para a memória.\n");
        return 0;
    }
    if (p->carros > NUM_VIAS * p->tamanho_via) {
        fprintf(stderr, "CARROS não pode passar de 4 * TAMANHO_VIA.\n");
        return 0;
    }
    return 1;
}

static DadosCompartilhados *criar_memoria_compartilhada(size_t tamanho,
                                                       int num_processos) {
    DadosCompartilhados *dados;
    pthread_barrierattr_t atributos;
    int barreira_criada;

    dados = mmap(NULL, tamanho, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (dados == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    /* O segundo argumento 1 indica semáforo compartilhado entre processos. */
    if (sem_init(&dados->cruzamento.mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(dados, tamanho);
        return NULL;
    }

    pthread_barrierattr_init(&atributos);
    pthread_barrierattr_setpshared(&atributos, PTHREAD_PROCESS_SHARED);
    barreira_criada = pthread_barrier_init(&dados->barreira, &atributos,
                                           (unsigned)num_processos) == 0;
    pthread_barrierattr_destroy(&atributos);
    if (!barreira_criada) {
        fprintf(stderr, "Não foi possível criar a barreira.\n");
        sem_destroy(&dados->cruzamento.mutex);
        munmap(dados, tamanho);
        return NULL;
    }

    return dados;
}

int main(int argc, char *argv[]) {
    Parametros p;
    int num_processos;
    uint64_t total;
    size_t tamanho_mapa;
    DadosCompartilhados *dados;
    pid_t *pids;
    int criados = 0;
    int houve_erro = 0;
    int id;
    struct timespec inicio;
    struct timespec fim;

    if (argc != 7 || !ler_uint64(argv[1], &p.carros) ||
        !ler_uint64(argv[2], &p.tamanho_via) ||
        !ler_uint64(argv[3], &p.tempo_semaforo) ||
        !ler_uint64(argv[4], &p.iteracoes) ||
        !ler_num_processos(argv[5], &num_processos) ||
        !ler_uint64(argv[6], &p.seed)) {
        fprintf(stderr,
                "Uso: %s CARROS TAMANHO_VIA TEMPO_SEMAFORO ITERACOES "
                "PROCESSOS SEED\n",
                argv[0]);
        fprintf(stderr, "PROCESSOS deve estar entre 1 e %d.\n", MAX_PROCESSOS);
        return EXIT_FAILURE;
    }
    if (!validar_parametros(&p)) {
        return EXIT_FAILURE;
    }

    /* Dois estados das vias: o atual e o da próxima iteração. */
    total = NUM_VIAS * p.tamanho_via;
    tamanho_mapa = sizeof(DadosCompartilhados) + (size_t)(2 * total);
    dados = criar_memoria_compartilhada(tamanho_mapa, num_processos);
    if (dados == NULL) {
        return EXIT_FAILURE;
    }
    /* mmap anônimo já começa zerado: cruzamento vazio e contadores em zero. */
    distribuir_carros(&p, dados->celulas);

    pids = malloc((size_t)num_processos * sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "Não foi possível reservar memória.\n");
        pthread_barrier_destroy(&dados->barreira);
        sem_destroy(&dados->cruzamento.mutex);
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
            trabalho_do_processo(id, num_processos, &p, dados);
            _exit(EXIT_SUCCESS);
        }
        pids[criados++] = pid;
    }

    /* Os filhos já criados ficariam presos na barreira esperando os demais. */
    if (houve_erro) {
        for (id = 0; id < criados; id++) {
            kill(pids[id], SIGKILL);
        }
    }

    for (id = 0; id < criados; id++) {
        int status;

        if (waitpid(pids[id], &status, 0) == -1 || !WIFEXITED(status) ||
            WEXITSTATUS(status) != EXIT_SUCCESS) {
            houve_erro = 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);

    if (!houve_erro) {
        mostrar_resultado(&p, &dados->cruzamento,
                          dados->celulas + (p.iteracoes % 2) * total,
                          num_processos, diferenca_em_segundos(inicio, fim));
    }

    free(pids);
    pthread_barrier_destroy(&dados->barreira);
    sem_destroy(&dados->cruzamento.mutex);
    munmap(dados, tamanho_mapa);
    return houve_erro ? EXIT_FAILURE : EXIT_SUCCESS;
}
