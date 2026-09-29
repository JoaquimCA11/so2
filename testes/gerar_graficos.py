#!/usr/bin/env python3
"""Calcula médias e speedup a partir dos CSVs e gera os gráficos em SVG.

Uso: python3 testes/gerar_graficos.py

Lê testes/resultados/tempos_fora_container.csv (obrigatório) e
testes/resultados/tempos_container.csv (opcional) e grava em
testes/resultados/:
  grafico1_tempo.svg     tempo médio das três versões
  grafico2_speedup.svg   speedup de processos e threads
  grafico3_container.svg versões paralelas fora x dentro do container
  resumo.md              tabelas com os valores usados nos gráficos
Usa somente a biblioteca padrão do Python.
"""

import csv
import statistics
from pathlib import Path

DIRETORIO_TESTES = Path(__file__).resolve().parent
RESULTADOS = DIRETORIO_TESTES / "resultados"
ENTRADA = DIRETORIO_TESTES / "entradas" / "entrada_padrao.txt"
CSV_FORA = RESULTADOS / "tempos_fora_container.csv"
CSV_CONTAINER = RESULTADOS / "tempos_container.csv"

# Cores e tipografia dos gráficos
FUNDO = "#fcfcfb"
TEXTO = "#0b0b0b"
TEXTO_SECUNDARIO = "#52514e"
TEXTO_APAGADO = "#898781"
GRADE = "#e1e0d9"
EIXO = "#c3c2b7"
COR_VERSAO = {"sequencial": "#1baf7a", "processos": "#2a78d6", "threads": "#eb6834"}
NOME_VERSAO = {"sequencial": "Sequencial", "processos": "Processos", "threads": "Threads"}
COR_FORA = TEXTO_APAGADO
COR_CPUS = {"1": "#86b6ef", "2": "#2a78d6", "4": "#104281"}
FONTE = "system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif"

LARGURA = 760
ALTURA = 460


def numero(valor, casas=2):
    return f"{valor:.{casas}f}".replace(".", ",")


def ler_entrada():
    valores = {}
    for linha in ENTRADA.read_text(encoding="utf-8").splitlines():
        linha = linha.strip()
        if linha and not linha.startswith("#") and "=" in linha:
            chave, valor = linha.split("=", 1)
            valores[chave.strip()] = valor.strip()
    return valores


def ler_tempos(caminho, chaves):
    """Agrupa as repetições pela combinação das colunas em `chaves`."""
    grupos = {}
    with open(caminho, newline="", encoding="utf-8") as arquivo:
        for linha in csv.DictReader(arquivo):
            chave = tuple(linha[c] for c in chaves)
            grupos.setdefault(chave, []).append(float(linha["tempo"]))
    return {
        chave: (statistics.mean(tempos), statistics.pstdev(tempos), len(tempos))
        for chave, tempos in grupos.items()
    }


# ---------------------------------------------------------------- SVG básico

def texto(x, y, conteudo, tamanho=12, cor=TEXTO_SECUNDARIO, ancora="start",
          peso="normal"):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-size="{tamanho}" fill="{cor}" '
            f'text-anchor="{ancora}" font-weight="{peso}">{conteudo}</text>')


def barra(x, topo, largura, base, cor):
    """Barra com os cantos de cima arredondados e a base reta no eixo."""
    altura = base - topo
    raio = min(4, largura / 2, altura)
    return (f'<path d="M{x:.1f},{base:.1f} L{x:.1f},{topo + raio:.1f} '
            f'Q{x:.1f},{topo:.1f} {x + raio:.1f},{topo:.1f} '
            f'L{x + largura - raio:.1f},{topo:.1f} '
            f'Q{x + largura:.1f},{topo:.1f} {x + largura:.1f},{topo + raio:.1f} '
            f'L{x + largura:.1f},{base:.1f} Z" fill="{cor}"/>')


def legenda(x, y, itens):
    partes = []
    for nome, cor in itens:
        partes.append(f'<rect x="{x}" y="{y - 10}" width="12" height="12" rx="3" fill="{cor}"/>')
        partes.append(texto(x + 18, y, nome, 13))
        x += 18 + 8 * len(nome) + 24
    return partes


def documento(titulo, subtitulo, corpo):
    return "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{LARGURA}" height="{ALTURA}" '
        f'viewBox="0 0 {LARGURA} {ALTURA}" font-family="{FONTE}">',
        f'<rect width="{LARGURA}" height="{ALTURA}" fill="{FUNDO}"/>',
        texto(32, 40, titulo, 20, TEXTO, peso="600"),
        texto(32, 64, subtitulo, 13),
        *corpo,
        "</svg>",
    ])


def escala_vertical(maximo):
    """Escolhe um passo "redondo" para a grade e o topo do eixo."""
    for passo in (0.25, 0.5, 1, 2, 5, 10, 20, 50):
        if maximo / passo <= 6:
            return passo, passo * (int(maximo / passo) + 1)
    return maximo / 5, maximo


def eixo_y(esquerda, direita, topo, base, maximo, passo, rotulo, casas):
    partes = []
    valor = 0.0
    while valor <= maximo + 1e-9:
        y = base - (base - topo) * valor / maximo
        cor = EIXO if valor == 0 else GRADE
        partes.append(f'<line x1="{esquerda}" x2="{direita}" y1="{y:.1f}" '
                      f'y2="{y:.1f}" stroke="{cor}" stroke-width="1"/>')
        partes.append(texto(esquerda - 8, y + 4, numero(valor, casas), 12,
                            TEXTO_APAGADO, "end"))
        valor += passo
    partes.append(f'<text transform="translate({esquerda - 46},{(topo + base) / 2:.1f}) '
                  f'rotate(-90)" font-size="12" fill="{TEXTO_SECUNDARIO}" '
                  f'text-anchor="middle">{rotulo}</text>')
    return partes


# ------------------------------------------------------------------ gráficos

def grafico_tempo(fora, subtitulo):
    esquerda, direita, topo, base = 96, LARGURA - 32, 110, ALTURA - 70
    grupos = [("Sequencial", [("sequencial", "1")])] + [
        (n, [("processos", n), ("threads", n)]) for n in ("1", "2", "4", "8")
    ]
    maximo_dado = max(media for media, _, _ in fora.values())
    passo, maximo = escala_vertical(maximo_dado)
    corpo = legenda(32, 92, [(NOME_VERSAO[v], COR_VERSAO[v])
                             for v in ("sequencial", "processos", "threads")])
    corpo += eixo_y(esquerda, direita, topo, base, maximo, passo,
                    "Tempo médio (s)", 1)

    largura_grupo = (direita - esquerda) / len(grupos)
    largura_barra = min(44, largura_grupo * 0.3)
    for indice, (nome, barras) in enumerate(grupos):
        centro = esquerda + largura_grupo * (indice + 0.5)
        inicio = centro - (len(barras) * largura_barra + (len(barras) - 1) * 2) / 2
        for posicao, (versao, trabalhadores) in enumerate(barras):
            media = fora[(versao, trabalhadores)][0]
            x = inicio + posicao * (largura_barra + 2)
            y = base - (base - topo) * media / maximo
            corpo.append(barra(x, y, largura_barra, base, COR_VERSAO[versao]))
            if versao == "sequencial" or trabalhadores == "8":
                corpo.append(texto(x + largura_barra / 2, y - 6, numero(media),
                                   12, TEXTO, "middle"))
        corpo.append(texto(centro, base + 20, nome, 12, TEXTO_SECUNDARIO, "middle"))
    corpo.append(texto((esquerda + direita) / 2, base + 44,
                       "Trabalhadores (processos ou threads)", 12,
                       TEXTO_SECUNDARIO, "middle"))
    return documento("Gráfico 1 — Tempo de execução", subtitulo, corpo)


def grafico_speedup(fora, subtitulo):
    esquerda, direita, topo, base = 96, LARGURA - 150, 110, ALTURA - 70
    sequencial = fora[("sequencial", "1")][0]
    maximo = 8.0

    def ponto(trabalhadores, speedup):
        x = esquerda + (direita - esquerda) * trabalhadores / maximo
        y = base - (base - topo) * speedup / maximo
        return x, y

    corpo = legenda(32, 92, [(NOME_VERSAO[v], COR_VERSAO[v])
                             for v in ("processos", "threads")])
    corpo += eixo_y(esquerda, direita, topo, base, maximo, 1, "Speedup", 0)
    for trabalhadores in (1, 2, 4, 8):
        x, _ = ponto(trabalhadores, 0)
        corpo.append(texto(x, base + 20, str(trabalhadores), 12,
                           TEXTO_SECUNDARIO, "middle"))

    # Referência: speedup ideal igual ao número de trabalhadores
    x1, y1 = ponto(1, 1)
    x2, y2 = ponto(8, 8)
    corpo.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
                 f'stroke="{TEXTO_APAGADO}" stroke-width="1.5"/>')
    corpo.append(texto(x2 + 8, y2 + 4, "Ideal (linear)", 12, TEXTO_APAGADO))

    # As duas linhas quase coincidem: threads usa quadrados cheios e processos
    # um anel maior desenhado por cima, para as duas continuarem visíveis.
    finais = []
    for versao in ("threads", "processos"):
        pontos = [ponto(n, sequencial / fora[(versao, str(n))][0])
                  for n in (1, 2, 4, 8)]
        caminho = " ".join(f"{x:.1f},{y:.1f}" for x, y in pontos)
        corpo.append(f'<polyline points="{caminho}" fill="none" '
                     f'stroke="{COR_VERSAO[versao]}" stroke-width="2" '
                     f'stroke-linejoin="round"/>')
        for x, y in pontos:
            if versao == "processos":
                corpo.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="8" '
                             f'fill="none" stroke="{COR_VERSAO[versao]}" '
                             f'stroke-width="2"/>')
            else:
                corpo.append(f'<rect x="{x - 4.5:.1f}" y="{y - 4.5:.1f}" width="9" '
                             f'height="9" rx="2" fill="{COR_VERSAO[versao]}" '
                             f'stroke="{FUNDO}" stroke-width="2"/>')
        finais.append((pontos[-1][1], versao, sequencial / fora[(versao, "8")][0]))

    # Rótulos no fim das linhas, afastados para não se sobreporem
    finais.sort()
    for ordem, (y, versao, speedup) in enumerate(finais):
        deslocamento = -9 if ordem == 0 else 17
        corpo.append(texto(direita + 12, y + deslocamento,
                           f"{NOME_VERSAO[versao]} {numero(speedup)}×", 12, TEXTO))

    corpo.append(texto((esquerda + direita) / 2, base + 44,
                       "Trabalhadores (processos ou threads)", 12,
                       TEXTO_SECUNDARIO, "middle"))
    return documento("Gráfico 2 — Speedup", subtitulo, corpo)


def grafico_container(fora, container, subtitulo):
    topo, base = 132, ALTURA - 70
    paineis = (("processos", 96, 400), ("threads", 440, LARGURA - 32))
    series = [("Fora do container", COR_FORA, None)] + [
        (f"--cpus={c}", COR_CPUS[c], c) for c in ("1", "2", "4")
    ]
    medias = [fora[(v, n)][0] for v in ("processos", "threads") for n in ("2", "4", "8")]
    medias += [media for media, _, _ in container.values()]
    passo, maximo = escala_vertical(max(medias))

    corpo = legenda(32, 92, [(nome, cor) for nome, cor, _ in series])
    for versao, esquerda, direita in paineis:
        corpo.append(texto(esquerda, 120, NOME_VERSAO[versao], 14, TEXTO, peso="600"))
        if versao == "processos":
            corpo += eixo_y(esquerda, direita, topo, base, maximo, passo,
                            "Tempo médio (s)", 1)
        else:
            corpo += [p for p in eixo_y(esquerda, direita, topo, base, maximo,
                                        passo, "", 1) if p.startswith("<line")]
        largura_grupo = (direita - esquerda) / 3
        largura_barra = min(22, (largura_grupo - 24) / 4)
        for indice, trabalhadores in enumerate(("2", "4", "8")):
            centro = esquerda + largura_grupo * (indice + 0.5)
            inicio = centro - (4 * largura_barra + 3 * 2) / 2
            for posicao, (_, cor, cpus) in enumerate(series):
                if cpus is None:
                    media = fora[(versao, trabalhadores)][0]
                else:
                    media = container.get((versao, cpus, trabalhadores), (None,))[0]
                if media is None:
                    continue
                x = inicio + posicao * (largura_barra + 2)
                y = base - (base - topo) * media / maximo
                corpo.append(barra(x, y, largura_barra, base, cor))
            corpo.append(texto(centro, base + 20, trabalhadores, 12,
                               TEXTO_SECUNDARIO, "middle"))
    corpo.append(texto(LARGURA / 2, base + 44, "Trabalhadores (processos ou threads)",
                       12, TEXTO_SECUNDARIO, "middle"))
    return documento("Gráfico 3 — Fora x dentro do container", subtitulo, corpo)


# -------------------------------------------------------------------- resumo

def tabela(cabecalho, linhas):
    saida = ["| " + " | ".join(cabecalho) + " |",
             "|" + "|".join("---" for _ in cabecalho) + "|"]
    saida += ["| " + " | ".join(linha) + " |" for linha in linhas]
    return "\n".join(saida)


def escrever_resumo(entrada, fora, container):
    sequencial = fora[("sequencial", "1")][0]
    partes = [
        "# Resumo dos resultados",
        "",
        "Gerado por `testes/gerar_graficos.py` a partir dos CSVs desta pasta.",
        "",
        "Entrada: " + ", ".join(f"{k}={v}" for k, v in entrada.items()),
        "",
        "## Fora do container",
        "",
        "Speedup = tempo médio sequencial / tempo médio paralelo. "
        "Eficiência = speedup / trabalhadores.",
        "",
    ]
    linhas = []
    for versao in ("sequencial", "processos", "threads"):
        for trabalhadores in ("1", "2", "4", "8"):
            if (versao, trabalhadores) not in fora:
                continue
            media, desvio, n = fora[(versao, trabalhadores)]
            speedup = sequencial / media
            linhas.append([NOME_VERSAO[versao], trabalhadores, str(n),
                           numero(media, 3), numero(desvio, 3), numero(speedup),
                           numero(100 * speedup / int(trabalhadores), 0) + "%"])
    partes.append(tabela(["Versão", "Trabalhadores", "Execuções", "Tempo médio (s)",
                          "Desvio padrão (s)", "Speedup", "Eficiência"], linhas))

    partes += ["", "## Dentro do container", ""]
    if not container:
        partes.append("Ainda não há `tempos_container.csv`. Rode "
                      "`./testes/executar_testes_container.sh` e gere de novo.")
    else:
        partes += [
            "Speedup no container = sequencial no mesmo limite de CPU / paralelo. "
            "Dentro/fora = tempo no container / tempo fora do container.",
            "",
        ]
        linhas = []
        for versao in ("sequencial", "processos", "threads"):
            for cpus in ("1", "2", "4"):
                for trabalhadores in ("1", "2", "4", "8"):
                    chave = (versao, cpus, trabalhadores)
                    if chave not in container:
                        continue
                    media, desvio, n = container[chave]
                    base = container.get(("sequencial", cpus, "1"))
                    speedup = numero(base[0] / media) if base else "—"
                    referencia = fora.get((versao, trabalhadores))
                    razao = numero(media / referencia[0]) if referencia else "—"
                    linhas.append([NOME_VERSAO[versao], cpus, trabalhadores, str(n),
                                   numero(media, 3), numero(desvio, 3), speedup, razao])
        partes.append(tabela(["Versão", "CPUs", "Trabalhadores", "Execuções",
                              "Tempo médio (s)", "Desvio padrão (s)",
                              "Speedup no container", "Dentro/fora"], linhas))
    (RESULTADOS / "resumo.md").write_text("\n".join(partes) + "\n", encoding="utf-8")


def main():
    entrada = ler_entrada()
    fora = ler_tempos(CSV_FORA, ["versao", "trabalhadores"])
    container = {}
    if CSV_CONTAINER.exists():
        container = ler_tempos(CSV_CONTAINER, ["versao", "cpus", "trabalhadores"])

    subtitulo = (f"{entrada['CARROS']} carros, vias de {entrada['TAMANHO_VIA']} "
                 f"posições, semáforo de {entrada['TEMPO_SEMAFORO']} iterações, "
                 f"{entrada['ITERACOES']} iterações — média das execuções")

    (RESULTADOS / "grafico1_tempo.svg").write_text(
        grafico_tempo(fora, subtitulo), encoding="utf-8")
    (RESULTADOS / "grafico2_speedup.svg").write_text(
        grafico_speedup(fora, subtitulo), encoding="utf-8")
    if container:
        (RESULTADOS / "grafico3_container.svg").write_text(
            grafico_container(fora, container, subtitulo), encoding="utf-8")
    else:
        print("Sem tempos_container.csv: gráfico 3 não gerado.")
    escrever_resumo(entrada, fora, container)
    print(f"Gráficos e resumo gravados em {RESULTADOS}")


if __name__ == "__main__":
    main()
