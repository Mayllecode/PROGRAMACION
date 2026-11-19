#!/usr/bin/env python3
"""
graficos_s11.py — Figuras de las semanas 9 a 11 (MPI).

Uso (desde la raíz del repositorio):
    python scripts/graficos_s11.py       # o: make figuras-s11

Lee los CSV más recientes de results/ y salta lo que falte:
    mpi_<máquina>.csv       <- make bench-mpi   (Cannon y SUMMA)
    mpi_sim_<máquina>.csv   <- make bench-mpi   (simulador repartido)
    omp_sim_<máquina>.csv   <- make bench-omp   (para comparar con OpenMP, opcional)

Escribe en results/figuras/:
    10_mpi_denso.png        S(p) de Cannon y SUMMA, y en qué se va el tiempo
    11_mpi_comunicacion.png mensajes y bytes: Cannon contra SUMMA
    12_mpi_simulador.png    S(p) del simulador repartido (y OpenMP de S7)

Además imprime las tablas de S(p) y E(p) que van en docs/GUIA_S9_S11.md.
"""
import csv
import os
import statistics
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graficos import (INK2, MARKERS, MUTED, OUT, SERIES, latest, ring, save,  # noqa: E402
                      setup_style, skip, suptitle)
import matplotlib.pyplot as plt  # noqa: E402


def load(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def med(rows, key, field="tiempo_s"):
    """{clave: mediana del campo}, con clave = key(fila)."""
    g = defaultdict(list)
    for r in rows:
        g[key(r)].append(float(r[field]))
    return {k: statistics.median(v) for k, v in g.items()}


PASIVA = os.path.join("results", "espera_pasiva")


def pasiva(pattern):
    """CSV medido con p = 16 y MSMPI_PROGRESS_SPIN_LIMIT=1 (make bench-mpi-pasiva), si existe."""
    import glob
    files = [f for f in glob.glob(os.path.join(PASIVA, pattern))
             if pattern.startswith("mpi_sim_") or not os.path.basename(f).startswith("mpi_sim_")]
    return load(max(files, key=os.path.getmtime)) if files else []


def ideal(ax, ps):
    pmax = max(ps)
    ax.plot([1, pmax], [1, pmax], color=MUTED, linestyle="--", linewidth=1.2, label="ideal: S(p) = p")
    ax.set_xticks(ps)
    ax.set_xlim(0.5, pmax * 1.12)
    ax.set_xlabel("p (procesos MPI)")
    ax.set_ylabel("Speedup S(p) = T(1) / T(p)")


def series(ax, ps, ss, i, label):
    ax.plot(ps, ss, color=SERIES[i], label=label)
    ring(ax, ps, ss, SERIES[i], MARKERS[i], 7.5)
    ax.annotate(f"{ss[-1]:.1f}×", (ps[-1], ss[-1]), textcoords="offset points", xytext=(8, -4),
                fontsize=9.5, color=SERIES[i], fontweight="semibold")


def fig_denso(rows, path):
    t = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])))
    comm = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "t_comm_s")
    comp = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "t_comp_s")
    dist = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "t_dist_s")
    ns = sorted({nn for (a, nn, p) in t if a == "cannon"})
    n = 1024 if 1024 in ns else ns[-1]   # n = 2048 no cabe en la caché L3 y mide sobre todo la RAM
    t1 = min(v for (al, nn, p), v in t.items() if nn == n and p == 1)   # T(1) conservador
    fig, (a, b) = plt.subplots(1, 2, figsize=(12.5, 5.6))
    allp = sorted({p for (al, nn, p) in t if nn == n and al in ("cannon", "summa")})
    for i, (alg, lab) in enumerate((("cannon", "Cannon (p = 1, 4, 9, 16)"), ("summa", "SUMMA (cualquier p)"))):
        ps = sorted(p for (al, nn, p) in t if al == alg and nn == n)
        series(a, ps, [t1 / t[(alg, n, p)] for p in ps], i, lab)
    pv = pasiva("mpi_*.csv")
    if pv:
        tp = med([r for r in pv if not r["algoritmo"].startswith("seq")], lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])))
        for i, alg in enumerate(("cannon", "summa")):
            for (al, nn, p), v in tp.items():
                if al == alg and nn == n:
                    a.plot([p], [t1 / v], linestyle="none", marker="*", markersize=13, markerfacecolor="none",
                           markeredgecolor=SERIES[i], markeredgewidth=1.6, zorder=6)
        a.plot([], [], linestyle="none", marker="*", markersize=12, markerfacecolor="none", markeredgecolor=INK2,
               label="p = 16 con espera pasiva")
    ideal(a, allp)
    a.set_ylim(0, 6)
    a.set_title(f"Speedup del producto denso, n = {n}")
    a.legend(loc="upper left")

    labels, xs = [], []
    x = 0
    for alg in ("cannon", "summa"):
        for p in sorted(p for (al, nn, p) in t if al == alg and nn == n):
            k = (alg, n, p)
            parts = [comp[k], comm[k], dist[k]]
            bottom = 0
            for j, (v, col) in enumerate(zip(parts, (SERIES[0], SERIES[1], SERIES[3]))):
                b.bar(x, v, bottom=bottom, color=col, width=0.72, edgecolor="#ffffff", linewidth=1)
                bottom += v
            labels.append(f"{'C' if alg == 'cannon' else 'S'}\np={p}")
            xs.append(x)
            x += 1
        x += 0.6
    b.set_xticks(xs)
    b.set_xticklabels(labels, fontsize=9)
    b.set_ylabel("Segundos (el proceso más lento)")
    b.set_title("En qué se va el tiempo (C = Cannon, S = SUMMA)")
    b.grid(axis="x", visible=False)
    for col, lab in ((SERIES[0], "calcular (producto local)"), (SERIES[1], "comunicar durante el cálculo"),
                     (SERIES[3], "repartir A, B y juntar C")):
        b.plot([], [], linewidth=8, color=col, label=lab)
    b.legend(loc="upper right")
    best_seq = min(((v, al) for (al, nn, p), v in t.items() if nn == n and p == 1))[1]
    suptitle(fig, "Cannon y SUMMA: el mismo producto repartido entre procesos",
             f"T(1) = {t1:.2f} s (lo más rápido con 1 proceso: {best_seq}). Tiempo de la llamada completa, incluido "
             "repartir desde el proceso 0 y juntar el resultado")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "10_mpi_denso.png", f"Datos: {path.replace(os.sep, '/')} (make bench-mpi), mediana de 5")


def fig_comunicacion(rows, path):
    msgs = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "mensajes")
    byts = med(rows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "bytes")
    n = max(nn for (a, nn, p) in msgs if a == "cannon")
    ps = sorted({p for (a, nn, p) in msgs if nn == n and a in ("cannon", "summa") and p > 1})
    fig, (a, b) = plt.subplots(1, 2, figsize=(12.5, 5.2))
    w = 0.38
    for i, alg in enumerate(("cannon", "summa")):
        xs = [k + (i - 0.5) * w for k, p in enumerate(ps) if (alg, n, p) in msgs]
        mm = [msgs[(alg, n, p)] for p in ps if (alg, n, p) in msgs]
        bb = [byts[(alg, n, p)] / 1e6 for p in ps if (alg, n, p) in msgs]
        lab = "Cannon (punto a punto)" if alg == "cannon" else "SUMMA (difusiones)"
        a.bar(xs, mm, width=w - 0.03, color=SERIES[i], label=lab, edgecolor="#ffffff")
        b.bar(xs, bb, width=w - 0.03, color=SERIES[i], label=lab, edgecolor="#ffffff")
        for x, v in zip(xs, mm):
            a.text(x, v, f"{v:.0f}", ha="center", va="bottom", fontsize=8.5, color=INK2)
        for x, v in zip(xs, bb):
            b.text(x, v, f"{v:.0f}", ha="center", va="bottom", fontsize=8.5, color=INK2)
    for ax in (a, b):
        ax.set_xticks(range(len(ps)))
        ax.set_xticklabels([f"p = {p}" for p in ps])
        ax.grid(axis="x", visible=False)
        ax.legend(loc="upper left")
    a.set_ylabel("Mensajes recibidos (todos los procesos)")
    a.set_title("Número de mensajes durante el cálculo")
    b.set_ylabel("MB recibidos (todos los procesos)")
    b.set_title("Volumen de datos durante el cálculo")
    suptitle(fig, f"Comunicación del producto denso, n = {n}",
             "Sin contar el reparto inicial ni la recolección final. Cannon no corre con p = 2 ni 8 "
             "(necesita una malla cuadrada). Su extra respecto a SUMMA es el sesgo inicial")
    fig.tight_layout(rect=(0, 0.02, 1, 0.88))
    save(fig, "11_mpi_comunicacion.png", f"Datos: {path.replace(os.sep, '/')} (make bench-mpi)")


def fig_simulador(rows, path):
    t = med(rows, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])))
    comm = med(rows, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])), "t_comm_s")
    sizes = sorted({(pi, va) for (a, pi, va, p) in t}, key=lambda s: s[0] * s[1])
    big = sizes[-1]
    fig, (a, b) = plt.subplots(1, 2, figsize=(12.5, 5.6))
    allp = sorted({p for (al, pi, va, p) in t if al.startswith("sim_mpi")})
    for i, (pi, va) in enumerate(sizes):
        t1 = min(v for (al, x, y, p), v in t.items() if (x, y) == (pi, va) and p == 1)   # T(1) conservador
        ps = sorted(p for (al, x, y, p) in t if al == "sim_mpi_nnz" and (x, y) == (pi, va))
        series(a, ps, [t1 / t[("sim_mpi_nnz", pi, va, p)] for p in ps], i, f"MPI, edificio {pi} × {va}")
    pv = pasiva("mpi_sim_*.csv")
    if pv:
        tp = med(pv, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])))
        for i, (pi, va) in enumerate(sizes):
            t1 = min(v for (al, x, y, p), v in t.items() if (x, y) == (pi, va) and p == 1)
            k = ("sim_mpi_nnz", pi, va, 16)
            if k in tp:
                a.plot([16], [t1 / tp[k]], linestyle="none", marker="*", markersize=13, markerfacecolor="none",
                       markeredgecolor=SERIES[i], markeredgewidth=1.6, zorder=6)
        a.plot([], [], linestyle="none", marker="*", markersize=12, markerfacecolor="none", markeredgecolor=INK2,
               label="p = 16 con espera pasiva")
    omp = latest("omp_sim_*.csv")
    if omp:
        orow = load(omp)
        ot = med(orow, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])))
        pi, va = big
        if ("sim_omp", pi, va, 1) in ot:
            t1o = min(ot[(s, pi, va, 1)] for s in ("sim_seq", "sim_seq_rowinit") if (s, pi, va, 1) in ot)
            ps = sorted(p for (al, x, y, p) in ot if al == "sim_omp" and (x, y) == (pi, va))
            a.plot(ps, [t1o / ot[("sim_omp", pi, va, p)] for p in ps], color=MUTED, linestyle=":",
                   marker="x", label=f"OpenMP (S7), {pi} × {va}")
    ideal(a, allp)
    a.set_ylim(0, 6)
    a.set_title("Speedup del simulador (reparto por no nulos)")
    a.legend(loc="upper left", fontsize=9)

    for i, (pi, va) in enumerate(sizes):
        ps = sorted(p for (al, x, y, p) in t if al == "sim_mpi_nnz" and (x, y) == (pi, va) and p > 1)
        fr = [100 * comm[("sim_mpi_nnz", pi, va, p)] / t[("sim_mpi_nnz", pi, va, p)] for p in ps]
        b.plot(ps, fr, color=SERIES[i], label=f"{pi} × {va}")
        ring(b, ps, fr, SERIES[i], MARKERS[i], 7.5)
    b.set_xticks([p for p in allp if p > 1])
    b.set_xlabel("p (procesos MPI)")
    b.set_ylabel("% del tiempo intercambiando fronteras")
    b.set_ylim(0, 100)
    b.set_title("Cuánto pesa la comunicación")
    b.legend(loc="upper left")
    suptitle(fig, "Simulación distribuida: cada proceso avanza su franja de pisos",
             "400 pasos, 8 sismos. Con 16 procesos hay más procesos que hilos (14): ver la estrella. T(1) = lo más rápido con 1 proceso (secuencial o MPI con p = 1), medido en la misma corrida")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "12_mpi_simulador.png", f"Datos: {path.replace(os.sep, '/')} (make bench-mpi), mediana de 5")


def tablas(drows, srows):
    if drows:
        t = med(drows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])))
        ms = med(drows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "mensajes")
        by = med(drows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "bytes")
        cm = med(drows, lambda r: (r["algoritmo"], int(r["n"]), int(r["p"])), "t_comm_s")
        print("\n| Algoritmo | n | p | malla | mediana [s] | S(p) | E(p) | comunicación [s] | mensajes | MB |")
        print("|---|---:|---:|---|---:|---:|---:|---:|---:|---:|")
        grids = {(r["algoritmo"], int(r["p"])): r["malla"] for r in drows}
        for n in sorted({n for (_, n, _) in t}):
            t1 = min(v for (al, nn, p), v in t.items() if nn == n and p == 1)   # T(1) conservador
            print(f"| T(1) | {n} | 1 | - | {t1:.4f} | 1.00 | 1.00 | - | - | - |")
            for alg in ("cannon", "summa"):
                for p in sorted(p for (a, nn, p) in t if a == alg and nn == n):
                    k = (alg, n, p)
                    print(f"| {alg} | {n} | {p} | {grids[(alg, p)]} | {t[k]:.4f} | {t1 / t[k]:.2f} | "
                          f"{t1 / t[k] / p:.2f} | {cm[k]:.4f} | {ms[k]:.0f} | {by[k] / 1e6:.1f} |")
    if srows:
        t = med(srows, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])))
        cm = med(srows, lambda r: (r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])), "t_comm_s")
        imb = {(r["algoritmo"], int(r["pisos"]), int(r["vanos"]), int(r["p"])): float(r["desbalance"]) for r in srows}
        print("\n| Edificio | versión | p | mediana [s] | S(p) | E(p) | % comunicación | desbalance nnz |")
        print("|---|---|---:|---:|---:|---:|---:|---:|")
        for pi, va in sorted({(pi, va) for (_, pi, va, _) in t}, key=lambda s: s[0] * s[1]):
            t1 = min(v for (al, x, y, p), v in t.items() if (x, y) == (pi, va) and p == 1)   # T(1) conservador
            print(f"| {pi} × {va} | T(1) | 1 | {t1:.4f} | 1.00 | 1.00 | - | - |")
            for alg in ("sim_mpi_filas", "sim_mpi_nnz"):
                for p in sorted(p for (a, x, y, p) in t if a == alg and (x, y) == (pi, va)):
                    k = (alg, pi, va, p)
                    print(f"| {pi} × {va} | {alg} | {p} | {t[k]:.4f} | {t1 / t[k]:.2f} | {t1 / t[k] / p:.2f} | "
                          f"{100 * cm[k] / t[k]:.0f} % | {imb[k]:.3f} |")


def main():
    if not os.path.isdir("results"):
        sys.exit("Correr desde la raíz del repositorio (no se encuentra results/)")
    os.makedirs(OUT, exist_ok=True)
    setup_style()
    print(f"Figuras en {OUT}/")
    dpath = latest("mpi_*.csv", exclude="mpi_sim_")
    spath = latest("mpi_sim_*.csv")
    drows = load(dpath) if dpath else []
    srows = load(spath) if spath else []
    if drows:
        fig_denso(drows, dpath)
        fig_comunicacion(drows, dpath)
    else:
        skip("10_mpi_denso.png y 11_mpi_comunicacion.png", "make bench-mpi")
    if srows:
        fig_simulador(srows, spath)
    else:
        skip("12_mpi_simulador.png", "make bench-mpi")
    tablas(drows, srows)


if __name__ == "__main__":
    main()
