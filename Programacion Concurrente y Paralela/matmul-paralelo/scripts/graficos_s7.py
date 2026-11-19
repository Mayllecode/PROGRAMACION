#!/usr/bin/env python3
"""
graficos_s7.py — Figuras de la semana 7: speedup S(p) y eficiencia E(p).

Uso (desde la raíz del repositorio):
    python3 scripts/graficos_s7.py       # o: make figuras-s7

Lee los CSV más recientes de results/ y salta lo que falte:
    omp_<máquina>.csv       <- make bench-omp   (producto denso y disperso)
    omp_sim_<máquina>.csv   <- make bench-omp   (simulador)
    go_<máquina>.csv        <- make go-bench

Escribe en results/figuras/:
    08_speedup_productos.png   S(p) del producto denso y de K·U (OpenMP y Go)
    09_speedup_simulador.png   S(p) del simulador: todo el paso vs. solo K·U (Amdahl)

Usa el mismo estilo que scripts/graficos.py.
"""
import csv
import os
import statistics
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graficos import (INK2, MARKERS, MUTED, OUT, RES, SERIES, latest, ring, save,  # noqa: E402
                      setup_style, skip, suptitle)
import matplotlib.pyplot as plt  # noqa: E402


def medians(path):
    """{(algoritmo, n, p): mediana de tiempo_s} y las filas crudas."""
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    g = defaultdict(list)
    for r in rows:
        g[(r["algoritmo"], int(r["n"]), int(r.get("p") or 1))].append(float(r["tiempo_s"]))
    return {k: statistics.median(v) for k, v in g.items()}, rows


def speedup_series(med, t1, algo, n):
    ps = sorted(p for (a, nn, p) in med if a == algo and nn == n)
    return ps, [t1 / med[(algo, n, p)] for p in ps]


def plot_series(ax, ps, ss, i, label, show_values=True):
    ax.plot(ps, ss, color=SERIES[i], label=label)
    ring(ax, ps, ss, SERIES[i], MARKERS[i], 7.5)
    if show_values:
        ax.annotate(f"{ss[-1]:.1f}×", (ps[-1], ss[-1]), textcoords="offset points", xytext=(8, -4),
                    fontsize=9.5, color=SERIES[i], fontweight="semibold")


def ideal(ax, pmax, xlabel="p (hilos o goroutines)"):
    ax.plot([1, pmax], [1, pmax], color=MUTED, linestyle="--", linewidth=1.2, label="ideal: S(p) = p")
    ax.set_xticks([1, 2, 4, 8] if pmax <= 8 else [1, 2, 4, 8, pmax])
    ax.set_xlim(0.7, pmax * 1.18)
    ax.set_ylim(0, pmax * 1.05)
    ax.set_xlabel(xlabel)
    ax.set_ylabel("Speedup S(p) = T(1) / T(p)")


def fig_productos():
    omp = latest("omp_*.csv", exclude="omp_sim_")
    if not omp:
        return skip("08_speedup_productos.png", "make bench-omp")
    med, _ = medians(omp)
    gomed = medians(latest("go_*.csv"))[0] if latest("go_*.csv") else {}

    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 5.6))
    # --- Denso: el n más grande medido ---
    n = max(nn for (alg, nn, p) in med if alg == "omp_blocked64")
    t1 = min(med[(s, n, 1)] for s in ("ikj", "blocked64") if (s, n, 1) in med)
    pmax = max(p for (alg, nn, p) in med if nn == n)
    for i, (alg, lab) in enumerate((("omp_blocked64", "OpenMP por bloques (64)"), ("omp_ikj", "OpenMP ikj (filas)"))):
        plot_series(a, *speedup_series(med, t1, alg, n), i, lab)
    note_go = ""
    if gomed:
        ng = max(nn for (alg, nn, p) in gomed if alg == "go_pool")
        plot_series(a, *speedup_series(gomed, gomed[("go_ikj", ng, 1)], "go_pool", ng), 2,
                    f"Go worker pool (n = {ng}, contra Go secuencial)")
        note_go = f" y {latest('go_*.csv').replace(os.sep, '/')}"
    ideal(a, pmax)
    a.set_title(f"Producto denso C = A·B, n = {n}")
    a.legend(loc="upper left")
    # --- Disperso: la malla más grande ---
    ns = max(nn for (alg, nn, p) in med if alg.startswith("spmm_csr_omp"))
    seqs = [k for k in med if k[1] == ns and k[2] == 1 and k[0].startswith("spmm_csr") and "omp" not in k[0]]
    t1s = min(med[k] for k in seqs)
    alg_omp = next(alg for (alg, nn, p) in med if alg.startswith("spmm_csr_omp"))
    plot_series(b, *speedup_series(med, t1s, alg_omp, ns), 0, "OpenMP (filas de K entre hilos)")
    if gomed:
        gs = max(nn for (alg, nn, p) in gomed if alg.startswith("go_spmm_pool"))
        gseq = next(k for k in gomed if k[1] == gs and k[2] == 1 and k[0].startswith("go_spmm_k"))
        galg = next(alg for (alg, nn, p) in gomed if alg.startswith("go_spmm_pool"))
        plot_series(b, *speedup_series(gomed, gomed[gseq], galg, gs), 2, "Go worker pool")
    ideal(b, pmax)
    b.set_title("Producto disperso K·U, n = " + f"{ns:,}".replace(",", " ") + " nodos")
    b.legend(loc="upper left")
    suptitle(fig, "Semana 7: speedup del producto de matrices en memoria compartida",
             "T(1) = la mejor versión secuencial (no la paralela con 1 hilo). "
             "Pasar de 6 hilos usa los hilos lógicos del mismo núcleo (6 núcleos / 12 hilos)")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "08_speedup_productos.png", f"Datos: {omp.replace(os.sep, '/')}{note_go}, mediana de 5")


def fig_simulador():
    path = latest("omp_sim_*.csv")
    if not path:
        return skip("09_speedup_simulador.png", "make bench-omp")
    med, rows = medians(path)
    tk = defaultdict(list)
    for r in rows:
        tk[(r["algoritmo"], int(r["n"]))].append(float(r["t_kernel_s"]))
    dims = {int(r["n"]): (int(r["pisos"]), int(r["vanos"])) for r in rows}
    sizes = sorted(dims)

    def t1_f(n):
        best = min((med[(s, n, 1)], s) for s in ("sim_seq", "sim_seq_rowinit") if (s, n, 1) in med)
        return best[0], statistics.median(tk[(best[1], n)]) / best[0]

    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 5.6))
    n = sizes[-1]
    t1, f = t1_f(n)
    ps = sorted(p for (alg, nn, p) in med if alg == "sim_omp" and nn == n)
    pmax = max(ps)
    plot_series(a, *speedup_series(med, t1, "sim_omp", n), 0, "Todo el paso en paralelo (sim_run_omp)")
    plot_series(a, *speedup_series(med, t1, "sim_omp_soloKU", n), 1, "Solo K·U en paralelo")
    xs = [1 + (pmax - 1) * t / 100 for t in range(101)]
    a.plot(xs, [1 / ((1 - f) + f / x) for x in xs], color=SERIES[1], linestyle=":", linewidth=1.5,
           label=f"Límite de Amdahl con f = {f:.0%} en K·U")
    ideal(a, pmax, "p (hilos)")
    pi, va = dims[n]
    a.set_title(f"Edificio {pi} × {va} (n = {n:,} nodos)".replace(",", " "))
    a.legend(loc="upper left")
    # --- E(p) por tamaño: la granularidad ---
    width = 0.8 / len(sizes)
    for i, nn in enumerate(sizes):
        t1n, _ = t1_f(nn)
        pn, sn = speedup_series(med, t1n, "sim_omp", nn)
        xs = [k + (i - (len(sizes) - 1) / 2) * width for k in range(len(pn))]
        pi, va = dims[nn]
        b.bar(xs, [s / p for s, p in zip(sn, pn)], width=width - 0.03, color=SERIES[i],
              label=f"{pi} × {va} (n = {nn:,})".replace(",", " "))
    b.axhline(1.0, color=MUTED, linestyle="--", linewidth=1.2)
    b.set_xticks(range(len(ps)))
    b.set_xticklabels([str(p) for p in ps])
    b.set_xlabel("p (hilos)")
    b.set_ylabel("Eficiencia E(p) = S(p) / p")
    b.set_title("Eficiencia del simulador completo por tamaño de edificio")
    b.grid(axis="x", visible=False)
    b.legend(loc="upper right")
    b.text(0.0, -0.2, "Edificio chico: poco trabajo por paso para repartir; crear y sincronizar hilos pesa más.",
           transform=b.transAxes, fontsize=9, color=INK2)
    suptitle(fig, "Semana 7: el simulador en paralelo y la ley de Amdahl",
             "Paralelizar solo K·U no pasa del límite de Amdahl; repartir también la actualización de U lo supera")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "09_speedup_simulador.png", f"Datos: {path.replace(os.sep, '/')} (make bench-omp), mediana de 5")


def main():
    if not os.path.isdir(RES):
        sys.exit("Correr desde la raíz del repositorio (no se encuentra results/)")
    os.makedirs(OUT, exist_ok=True)
    setup_style()
    print(f"Figuras en {OUT}/")
    fig_productos()
    fig_simulador()


if __name__ == "__main__":
    main()
