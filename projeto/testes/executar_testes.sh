#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C

DIRETORIO_TESTES=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RAIZ=$(CDPATH= cd -- "$DIRETORIO_TESTES/.." && pwd)
CSV="$DIRETORIO_TESTES/resultados.csv"
ITERACOES=${1:-10000000}
SEED=${2:-123}
REPETICOES=${3:-3}

if ! [[ "$ITERACOES" =~ ^[1-9][0-9]*$ && "$SEED" =~ ^[0-9]+$ && \
        "$REPETICOES" =~ ^[1-9][0-9]*$ ]]; then
    echo "Uso: $0 [ITERACOES] [SEED] [REPETICOES]" >&2
    exit 1
fi

make -C "$RAIZ/sequencial"
make -C "$RAIZ/processos"
make -C "$RAIZ/threads"

printf 'versao,trabalhadores,tempo\n' > "$CSV"
assinatura_referencia=""

obter_assinatura() {
    awk -F': ' '/^(Carros gerados|Carros que passaram|Carros esperando|Fila Norte|Fila Sul|Fila Leste|Fila Oeste):/ {print $1 "=" $2}'
}

registrar_execucao() {
    local versao=$1
    local trabalhadores=$2
    shift 2
    local saida
    local tempo
    local assinatura

    echo "Executando $versao com $trabalhadores trabalhador(es)..."
    saida=$("$@")
    tempo=$(printf '%s\n' "$saida" | awk '/Tempo de execução:/ {print $4}')
    assinatura=$(printf '%s\n' "$saida" | obter_assinatura)

    if [[ -z "$tempo" ]]; then
        echo "Não foi possível obter o tempo de $versao." >&2
        exit 1
    fi

    if [[ -z "$assinatura_referencia" ]]; then
        assinatura_referencia=$assinatura
    elif [[ "$assinatura" != "$assinatura_referencia" ]]; then
        echo "Erro: $versao produziu um resultado diferente da versão sequencial." >&2
        diff <(printf '%s\n' "$assinatura_referencia") \
             <(printf '%s\n' "$assinatura") || true
        exit 1
    fi

    printf '%s,%s,%s\n' "$versao" "$trabalhadores" "$tempo" >> "$CSV"
}

for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
    registrar_execucao sequencial 1 \
        "$RAIZ/sequencial/sequencial" "$ITERACOES" "$SEED"
done

for trabalhadores in 2 4 8; do
    for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
        registrar_execucao processos "$trabalhadores" \
            "$RAIZ/processos/processos" "$ITERACOES" "$trabalhadores" "$SEED"
    done
done

for trabalhadores in 2 4 8; do
    for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
        registrar_execucao threads "$trabalhadores" \
            "$RAIZ/threads/threads" "$ITERACOES" "$trabalhadores" "$SEED"
    done
done

echo "Testes concluídos. Resultados salvos em $CSV"
