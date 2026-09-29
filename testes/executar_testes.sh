#!/usr/bin/env bash
# Executa as três versões FORA do container, confere se processos e threads
# produzem o mesmo resultado da versão sequencial e grava os tempos em CSV.

set -euo pipefail
export LC_ALL=C

DIRETORIO_TESTES=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RAIZ=$(CDPATH= cd -- "$DIRETORIO_TESTES/.." && pwd)
ENTRADA=${1:-$DIRETORIO_TESTES/entradas/entrada_padrao.txt}
REPETICOES=${2:-3}
RESULTADOS="$DIRETORIO_TESTES/resultados"
CSV="$RESULTADOS/tempos_fora_container.csv"

# shellcheck source=entradas/entrada_padrao.txt
source "$ENTRADA"
ARGUMENTOS=("$CARROS" "$TAMANHO_VIA" "$TEMPO_SEMAFORO" "$ITERACOES")

if ! [[ "$REPETICOES" =~ ^[1-9][0-9]*$ ]]; then
    echo "Uso: $0 [ARQUIVO_DE_ENTRADA] [REPETICOES]" >&2
    exit 1
fi

make -C "$RAIZ/sequencial"
make -C "$RAIZ/processos"
make -C "$RAIZ/threads"

mkdir -p "$RESULTADOS"
printf 'versao,trabalhadores,repeticao,tempo\n' > "$CSV"
assinatura_referencia=""

obter_assinatura() {
    awk -F': ' '/^(Carros que atravessaram|Carros no cruzamento|Carros nas vias|Fila [A-Za-z]+):/ {print $1 "=" $2}'
}

registrar_execucao() {
    local versao=$1
    local trabalhadores=$2
    local repeticao=$3
    shift 3
    local saida
    local tempo
    local assinatura

    echo "Executando $versao com $trabalhadores trabalhador(es), repetição $repeticao..."
    saida=$("$@")
    tempo=$(printf '%s\n' "$saida" | awk '/Tempo de execução:/ {print $4}')
    assinatura=$(printf '%s\n' "$saida" | obter_assinatura)

    if [[ -z "$tempo" ]]; then
        echo "Não foi possível obter o tempo de $versao." >&2
        exit 1
    fi

    if [[ -z "$assinatura_referencia" ]]; then
        assinatura_referencia=$assinatura
        printf '%s\n' "$saida" > "$RESULTADOS/saida_sequencial.txt"
    elif [[ "$assinatura" != "$assinatura_referencia" ]]; then
        echo "Erro: $versao produziu um resultado diferente da versão sequencial." >&2
        diff <(printf '%s\n' "$assinatura_referencia") \
             <(printf '%s\n' "$assinatura") || true
        exit 1
    fi

    printf '%s,%s,%s,%s\n' "$versao" "$trabalhadores" "$repeticao" "$tempo" >> "$CSV"
}

for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
    registrar_execucao sequencial 1 "$repeticao" \
        "$RAIZ/sequencial/sequencial" "${ARGUMENTOS[@]}" "$SEED"
done

# 1 trabalhador mede o custo da estrutura paralela sem ganho de paralelismo.
for versao in processos threads; do
    for trabalhadores in 1 2 4 8; do
        for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
            registrar_execucao "$versao" "$trabalhadores" "$repeticao" \
                "$RAIZ/$versao/$versao" "${ARGUMENTOS[@]}" "$trabalhadores" "$SEED"
        done
    done
done

echo "Testes concluídos. Resultados iguais em todas as versões."
echo "Tempos salvos em $CSV"
