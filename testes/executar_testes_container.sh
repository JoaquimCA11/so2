#!/usr/bin/env bash
# Constrói a imagem e executa as versões DENTRO do container com limites de
# 1, 2 e 4 CPUs. As versões paralelas usam 2, 4 e 8 trabalhadores; a
# sequencial serve de referência para o resultado e para o speedup.
#
# Se o seu usuário não puder usar o Docker diretamente, rode com:
#   DOCKER="sudo docker" ./testes/executar_testes_container.sh

set -euo pipefail
export LC_ALL=C

DIRETORIO_TESTES=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RAIZ=$(CDPATH= cd -- "$DIRETORIO_TESTES/.." && pwd)
ENTRADA=${1:-$DIRETORIO_TESTES/entradas/entrada_padrao.txt}
REPETICOES=${2:-3}
RESULTADOS="$DIRETORIO_TESTES/resultados"
CSV="$RESULTADOS/tempos_container.csv"
IMAGEM=trabalho-paralela
read -r -a DOCKER <<< "${DOCKER:-docker}"

# shellcheck source=entradas/entrada_padrao.txt
source "$ENTRADA"
ARGUMENTOS=("$CARROS" "$TAMANHO_VIA" "$TEMPO_SEMAFORO" "$ITERACOES")

if ! [[ "$REPETICOES" =~ ^[1-9][0-9]*$ ]]; then
    echo "Uso: $0 [ARQUIVO_DE_ENTRADA] [REPETICOES]" >&2
    exit 1
fi

"${DOCKER[@]}" build -f "$RAIZ/container/Dockerfile" -t "$IMAGEM" "$RAIZ"

mkdir -p "$RESULTADOS"
printf 'versao,cpus,trabalhadores,repeticao,tempo\n' > "$CSV"
assinatura_referencia=""

obter_assinatura() {
    awk -F': ' '/^(Carros que atravessaram|Carros no cruzamento|Carros nas vias|Fila [A-Za-z]+):/ {print $1 "=" $2}'
}

registrar_execucao() {
    local versao=$1
    local cpus=$2
    local trabalhadores=$3
    local repeticao=$4
    shift 4
    local saida
    local tempo
    local assinatura

    echo "Container --cpus=$cpus: $versao com $trabalhadores trabalhador(es), repetição $repeticao..."
    saida=$("${DOCKER[@]}" run --rm --cpus="$cpus" "$IMAGEM" "$@")
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

    printf '%s,%s,%s,%s,%s\n' "$versao" "$cpus" "$trabalhadores" "$repeticao" \
        "$tempo" >> "$CSV"
}

for cpus in 1 2 4; do
    for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
        registrar_execucao sequencial "$cpus" 1 "$repeticao" \
            ./sequencial/sequencial "${ARGUMENTOS[@]}" "$SEED"
    done

    for versao in processos threads; do
        for trabalhadores in 2 4 8; do
            for ((repeticao = 1; repeticao <= REPETICOES; repeticao++)); do
                registrar_execucao "$versao" "$cpus" "$trabalhadores" "$repeticao" \
                    "./$versao/$versao" "${ARGUMENTOS[@]}" "$trabalhadores" "$SEED"
            done
        done
    done
done

echo "Testes no container concluídos. Resultados iguais em todas as versões."
echo "Tempos salvos em $CSV"
