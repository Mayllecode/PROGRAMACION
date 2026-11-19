#!/usr/bin/env python3
"""
graficos.py — Figuras del entregable a partir de los CSV de results/.

Uso (desde la raíz del repositorio):
    python3 scripts/graficos.py          # o: make figuras

Lee lo que exista en results/ y salta lo que falte, avisando qué comando lo genera:
    validacion_1gdl.csv, convergencia_1gdl.csv   <- make validar
    respuesta.csv, mapa_deriva.csv, historia_techo.csv   <- make demo
    t1_<máquina>.csv, t1_sim_<máquina>.csv       <- make bench

Escribe PNG en results/figuras/. Solo necesita matplotlib (pip install matplotlib).
"""
import csv
import glob
import math
import os
import statistics
import sys
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib import font_manager
    from matplotlib.colors import LinearSegmentedColormap, Normalize
except ImportError:
    sys.exit("Falta matplotlib: pip install matplotlib  (o sudo apt install python3-matplotlib)")

RES = "results"
OUT = os.path.join(RES, "figuras")

# ---- Paleta (validada: categórica en orden fijo, rampa secuencial de un solo tono) ----
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
MARKERS = ["o", "s", "^", "D", "v"]          # segundo canal: la identidad no depende solo del color
INK, INK2, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS, SURFACE = "#e1e0d9", "#c3c2b7", "#fcfcfb"
BLUE_RAMP = ["#cde2fb", "#b7d3f6", "#9ec5f4", "#86b6ef", "#6da7ec", "#5598e7", "#3987e5",
             "#2a78d6", "#256abf", "#1c5cab", "#184f95", "#104281", "#0d366b"]
SEQ = LinearSegmentedColormap.from_list("azul", BLUE_RAMP)

# Norma E.030 (Perú), tabla "Límites para la distorsión del entrepiso": concreto armado 0,007.
# Se aplica a derivas de diseño (inelásticas); aquí es solo una referencia visual.
E030_CONCRETO = 0.007

# Escenarios de la demo: color fijo por escenario (no por posición)
SCEN = {
    "sint_0.5Hz": ("Sintético 0.5 Hz", 0),
    "sint_1.0Hz": ("Sintético 1.0 Hz", 1),
    "sint_2.0Hz": ("Sintético 2.0 Hz", 2),
    "sint_4.0Hz": ("Sintético 4.0 Hz", 3),
    "registro":   ("Registro real", 4),
}


def scen_phrase(sid):
    """'sint_1.0Hz' -> 'sismo sintético de 1.0 Hz'; 'registro' -> 'registro real'."""
    if sid.startswith("sint_"):
        return f"sismo sintético de {sid[5:-2]} Hz"
    return "registro real" if sid == "registro" else sid


def setup_style():
    available = {f.name for f in font_manager.fontManager.ttflist}
    family = next((f for f in ("Segoe UI", "Inter", "Helvetica", "Arial", "DejaVu Sans") if f in available),
                  "DejaVu Sans")
    plt.rcParams.update({
        "font.family": family, "font.size": 11,
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": AXIS, "axes.linewidth": 0.8, "axes.labelcolor": INK2,
        "axes.titlecolor": INK, "axes.titlesize": 12, "axes.titleweight": "semibold",
        "axes.titlelocation": "left", "axes.titlepad": 10,
        "axes.spines.top": False, "axes.spines.right": False,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6, "grid.linestyle": "-",
        "axes.axisbelow": True,
        "xtick.color": MUTED, "ytick.color": MUTED, "xtick.labelcolor": INK2, "ytick.labelcolor": INK2,
        "xtick.major.size": 0, "ytick.major.size": 0,
        "legend.frameon": False, "legend.fontsize": 10, "legend.labelcolor": INK2,
        "lines.linewidth": 2.0, "lines.solid_capstyle": "round", "lines.solid_joinstyle": "round",
    })


def read_csv(name):
    path = os.path.join(RES, name)
    if not os.path.exists(path):
        return None
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def skip(fig_name, cmd):
    print(f"  [salto]  {fig_name}: faltan datos (correr {cmd})")


def save(fig, name, note=None):
    if note:
        fig.text(0.01, 0.01, note, color=MUTED, fontsize=8.5, ha="left", va="bottom")
    path = os.path.join(OUT, name)
    fig.savefig(path, dpi=200, bbox_inches="tight", pad_inches=0.25)
    plt.close(fig)
    print(f"  [OK]     {path}")


def suptitle(fig, title, subtitle):
    fig.suptitle(title, x=0.01, ha="left", fontsize=15, fontweight="semibold", color=INK, y=1.0)
    fig.text(0.01, 0.945, subtitle, ha="left", va="top", fontsize=10.5, color=INK2)


def ring(ax, x, y, color, marker="o", size=7):
    """Marcador con anillo del color de la superficie (se lee aunque cruce una línea)."""
    ax.plot(x, y, linestyle="none", marker=marker, markersize=size, markerfacecolor=color,
            markeredgecolor=SURFACE, markeredgewidth=1.6, zorder=5)


# ---------------------------------------------------------------------------
def fig_validacion():
    rows = read_csv("validacion_1gdl.csv")
    if not rows:
        return skip("01_validacion_1gdl.png", "make validar")
    t = [float(r["t_s"]) for r in rows]
    cols = {k: [float(r[k]) for r in rows] for k in ("u_sim", "u_exacta", "u_sim_amort", "u_exacta_amort")}
    err0 = [abs(a - b) for a, b in zip(cols["u_sim"], cols["u_exacta"])]
    err1 = [abs(a - b) for a, b in zip(cols["u_sim_amort"], cols["u_exacta_amort"])]

    fig, (a, b) = plt.subplots(2, 1, figsize=(10, 6.4), sharex=True, gridspec_kw={"height_ratios": [2.2, 1]})
    step = max(1, len(t) // 60)
    for key_e, key_s, color, mk, label in (("u_exacta", "u_sim", SERIES[0], "o", "sin amortiguamiento"),
                                           ("u_exacta_amort", "u_sim_amort", SERIES[1], "s", "con amortiguamiento (α = 2 s⁻¹)")):
        a.plot(t, cols[key_e], color=color, label=f"Exacta, {label}")
        ring(a, t[::step], cols[key_s][::step], color, mk, 6)
    a.plot([], [], linestyle="none", marker="o", markersize=6, markerfacecolor=MUTED, markeredgecolor=SURFACE,
           label="Puntos = simulación (diferencias centrales)")
    a.set_ylabel("Desplazamiento u(t) / u₀")
    a.set_title("Solución exacta (línea) y simulación (puntos)", pad=34)
    a.legend(loc="lower left", bbox_to_anchor=(0.0, 1.0), ncol=3, fontsize=9.5)
    b.plot(t, err0, color=SERIES[0], label="sin amortiguamiento")
    b.plot(t, err1, color=SERIES[1], label="con amortiguamiento")
    b.set_yscale("log")
    b.set_ylabel("|simulado − exacto|")
    b.set_xlabel("Tiempo [s]")
    b.set_title("Error absoluto: menor que 10⁻² en todo el intervalo")
    b.legend(loc="lower right", ncol=2)
    T = 2 * math.pi * math.sqrt(1000 / 4e5)
    suptitle(fig, "Hito H1: el simulador reproduce el oscilador de 1 GDL",
             f"m = 1000 kg, k = 4·10⁵ N/m  →  T = 2π√(m/k) = {T:.4f} s;  Δt = T/126,  5 periodos")
    fig.tight_layout(rect=(0, 0.02, 1, 0.92))
    save(fig, "01_validacion_1gdl.png", "Datos: results/validacion_1gdl.csv (make validar)")


def fig_convergencia():
    rows = read_csv("convergencia_1gdl.csv")
    if not rows:
        return skip("02_convergencia.png", "make validar")
    dt = [float(r["dt_s"]) for r in rows]
    err = [float(r["error_max"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8.5, 5.6))
    ref = [0.3 * err[0] * (d / dt[0]) ** 2 for d in dt]       # paralela, desplazada para que se vea
    ax.plot(dt, ref, color=AXIS, linewidth=1.4, zorder=1)
    mid = len(dt) // 2
    ax.annotate("referencia: pendiente 2 (teoría)", (dt[mid], ref[mid]), textcoords="offset points",
                xytext=(-6, -16), color=MUTED, fontsize=9.5, ha="right")
    ax.plot(dt, err, color=SERIES[0], zorder=2)
    ring(ax, dt, err, SERIES[0], "o", 8)
    for i in range(1, len(rows)):
        ax.annotate(f"÷{float(rows[i]['razon']):.2f}", (dt[i], err[i]), textcoords="offset points",
                    xytext=(7, 7), fontsize=9, color=INK2)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Paso de tiempo Δt [s]")
    ax.set_ylabel("Error máximo en 5 periodos")
    ax.set_xlim(dt[0] * 1.6, dt[-1] / 1.8)          # Δt decrece hacia la derecha
    suptitle(fig, "Convergencia de segundo orden: Δt a la mitad → error ÷ 4",
             "Cada etiqueta es error(2Δt) / error(Δt); la teoría del método de diferencias centrales predice 4")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "02_convergencia.png", "Datos: results/convergencia_1gdl.csv (make validar)")


def by_scenario(rows, value_key):
    data = defaultdict(dict)
    for r in rows:
        data[r["escenario"]][int(r["piso"])] = float(r[value_key])
    return data


def profile(ax, data, scale, xlabel, title):
    for sid, floors in data.items():
        label, slot = SCEN.get(sid, (sid, 4))
        pisos = sorted(floors)
        vals = [floors[p] * scale for p in pisos]
        ax.plot(vals, pisos, color=SERIES[slot], label=label, zorder=3)
        ring(ax, vals, pisos, SERIES[slot], MARKERS[slot], 6.5)
    ax.set_xlabel(xlabel)
    ax.set_title(title)
    ax.set_ylabel("Piso")
    top = max(max(f) for f in data.values())
    ax.set_yticks(range(1, top + 1))
    ax.set_ylim(0.5, top + 0.6)
    ax.set_xlim(left=0)


def fig_respuesta():
    rows = read_csv("respuesta.csv")
    if not rows:
        return skip("03_respuesta_por_piso.png", "make demo")
    u = by_scenario(rows, "desp_max_m")
    d = by_scenario(rows, "deriva_max")
    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 6.2), sharey=True)
    profile(a, u, 100.0, "Desplazamiento máximo [cm]", "Desplazamiento máximo (relativo al suelo)")
    profile(b, d, 1.0, "Deriva máxima  Δ/h", "Deriva de entrepiso (desplazamiento relativo / altura)")
    b.axvline(E030_CONCRETO, color=MUTED, linewidth=1.2, zorder=2)
    top = max(max(f) for f in d.values())
    for ax in (a, b):
        ax.set_ylim(0.5, top + 1.3)
    b.text(E030_CONCRETO, top + 0.85, "  E.030 concreto armado: 0,007 (solo referencia)", color=INK2,
           fontsize=9, va="center", ha="left")
    b.set_ylabel("")
    # etiqueta directa solo en la serie protagonista (la de mayor respuesta)
    sid_max = max(u, key=lambda s: max(u[s].values()))
    p_top = max(u[sid_max])
    a.annotate(f"{u[sid_max][p_top] * 100:.1f} cm en el techo", (u[sid_max][p_top] * 100, p_top),
               textcoords="offset points", xytext=(-10, 10), ha="right", fontsize=9.5, color=INK)
    handles, labels = a.get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=len(labels), bbox_to_anchor=(0.5, 0.905))
    suptitle(fig, f"Respuesta del edificio: el {scen_phrase(sid_max)} entra en resonancia",
             "Edificio de 10 pisos × 4 vanos (T1 ≈ 1.0 s), 4 sismos de 0.3 g simulados a la vez con un solo producto K·U por paso")
    fig.tight_layout(rect=(0, 0.02, 1, 0.84))
    save(fig, "03_respuesta_por_piso.png",
         "Datos: results/respuesta.csv (make demo). La norma aplica a derivas de diseño; este modelo es lineal y elástico.")


def fig_historia():
    rows = read_csv("historia_techo.csv")
    if not rows:
        return skip("04_historia_techo.png", "make demo")
    t = [float(r["t_s"]) for r in rows]
    sids = [k[len("u_techo_"):] for k in rows[0] if k.startswith("u_techo_")]
    n = len(sids)
    ag = {s: [float(r["ag_" + s]) / 9.81 for r in rows] for s in sids}
    ut = {s: [float(r["u_techo_" + s]) * 100 for r in rows] for s in sids}
    a_lim = max(max(abs(v) for v in ag[s]) for s in sids) * 1.1
    u_lim = max(max(abs(v) for v in ut[s]) for s in sids) * 1.1
    fig, axes = plt.subplots(n, 2, figsize=(12, 1.55 * n + 1.6), sharex=True, squeeze=False)
    for i, s in enumerate(sids):
        label, slot = SCEN.get(s, (s, 4))
        c = SERIES[slot]
        a, b = axes[i]
        a.plot(t, ag[s], color=c, linewidth=1.2)
        b.plot(t, ut[s], color=c, linewidth=1.5)
        a.set_ylim(-a_lim, a_lim)
        b.set_ylim(-u_lim, u_lim)
        a.set_ylabel(label.replace("Sintético ", ""), rotation=0, ha="right", va="center", color=INK2)
        peak = max(ut[s], key=abs)
        b.text(0.995, 0.92, f"máx {abs(peak):.1f} cm", transform=b.transAxes, ha="right", va="top",
               fontsize=9.5, color=INK)
    axes[0][0].set_title("Aceleración del suelo a_g(t) [g]")
    axes[0][1].set_title("Desplazamiento del techo [cm]  (misma escala en todas las filas)")
    axes[-1][0].set_xlabel("Tiempo [s]")
    axes[-1][1].set_xlabel("Tiempo [s]")
    suptitle(fig, "Misma aceleración pico, respuestas muy distintas",
             "Los cuatro sismos tienen 0.3 g; el de 1 Hz coincide con la frecuencia propia del edificio (f1 ≈ 1.0 Hz)")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "04_historia_techo.png", "Datos: results/historia_techo.csv (make demo)")


def fig_mapa():
    rows = read_csv("mapa_deriva.csv")
    resp = read_csv("respuesta.csv")
    if not rows or not resp:
        return skip("05_mapa_demanda.png", "make demo")
    d_floor = by_scenario(resp, "deriva_max")
    sids = list(d_floor)
    pisos = max(int(r["piso"]) for r in rows)
    vanos = max(int(r["columna"]) for r in rows)
    worst = max(sids, key=lambda s: max(d_floor[s].values()))
    vmax = max(max(f.values()) for f in d_floor.values())
    norm = Normalize(vmin=0.0, vmax=vmax)

    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 6.6), gridspec_kw={"width_ratios": [1.05, 1]})
    # (a) Pórtico: columnas pintadas con la deriva de su entrepiso, en el peor escenario
    node_d = {(int(r["piso"]), int(r["columna"])): float(r["deriva_max"]) for r in rows if r["escenario"] == worst}
    for p in range(1, pisos + 1):
        for c in range(vanos + 1):
            a.plot([c, c], [p - 1, p], color=SEQ(norm(node_d[(p, c)])), linewidth=5, solid_capstyle="butt", zorder=2)
        a.plot([0, vanos], [p, p], color=AXIS, linewidth=2.2, zorder=1)
    for p in range(1, pisos + 1):
        a.plot(range(vanos + 1), [p] * (vanos + 1), linestyle="none", marker="o", markersize=5.5,
               markerfacecolor=INK2, markeredgecolor=SURFACE, markeredgewidth=1.2, zorder=3)
    a.plot([-0.6, vanos + 0.6], [0, 0], color=INK2, linewidth=2)
    for x in [i * 0.3 - 0.6 for i in range(int((vanos + 1.2) / 0.3) + 1)]:
        a.plot([x, x - 0.2], [0, -0.25], color=MUTED, linewidth=0.8)
    p_crit = max(d_floor[worst], key=d_floor[worst].get)
    a.annotate(f"Entrepiso más exigido: piso {p_crit}\nΔ/h = {d_floor[worst][p_crit]:.4f}",
               (vanos, p_crit - 0.5), xytext=(vanos + 0.35, p_crit + 0.6), fontsize=9.5, color=INK,
               arrowprops=dict(arrowstyle="-", color=MUTED, linewidth=0.8))
    a.set_xlim(-0.8, vanos + 2.6)
    a.set_ylim(-0.5, pisos + 0.5)
    a.set_yticks(range(1, pisos + 1))
    a.set_xticks([])
    a.grid(False)
    a.spines["bottom"].set_visible(False)
    a.set_ylabel("Piso")
    a.set_title(f"Pórtico ante el {scen_phrase(worst)} (peor escenario)")

    # (b) Mapa piso × escenario
    grid = [[d_floor[s][p] for s in sids] for p in range(pisos, 0, -1)]
    im = b.imshow(grid, cmap=SEQ, norm=norm, aspect="auto", interpolation="nearest")
    b.set_xticks(range(len(sids)))
    b.set_xticklabels([SCEN.get(s, (s,))[0].replace("Sintético ", "") for s in sids])
    b.set_yticks(range(pisos))
    b.set_yticklabels([str(p) for p in range(pisos, 0, -1)])
    b.grid(False)
    for j, s in enumerate(sids):                      # etiqueta solo el piso crítico de cada escenario
        p = max(d_floor[s], key=d_floor[s].get)
        v = d_floor[s][p]
        txt_color = "#ffffff" if norm(v) > 0.55 else INK
        b.text(j, pisos - p, f"{v:.4f}", ha="center", va="center", fontsize=9, color=txt_color, fontweight="semibold")
    b.set_title("Deriva máxima por piso y escenario (valor = piso crítico)")
    b.set_ylabel("Piso")
    cb = fig.colorbar(im, ax=b, fraction=0.05, pad=0.03)
    cb.set_label("Deriva Δ/h", color=INK2)
    cb.outline.set_visible(False)
    cb.ax.tick_params(colors=MUTED, labelcolor=INK2)
    suptitle(fig, "Mapa de demanda: dónde se deforma más la estructura",
             "La deriva se concentra en los pisos bajos con el sismo resonante; los sismos de alta frecuencia exigen más a los pisos medios y altos")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "05_mapa_demanda.png", "Datos: results/mapa_deriva.csv y results/respuesta.csv (make demo)")


def latest(pattern, exclude=None):
    files = [f for f in glob.glob(os.path.join(RES, pattern)) if not (exclude and exclude in os.path.basename(f))]
    return max(files, key=os.path.getmtime) if files else None


def fig_t1_denso():
    path = latest("t1_*.csv", exclude="t1_sim_")
    if not path:
        return skip("06_t1_denso.png", "make bench")
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    times = defaultdict(lambda: defaultdict(list))
    for r in rows:
        if r["algoritmo"].startswith("spmm"):
            continue
        times[r["algoritmo"]][int(r["n"])].append(float(r["tiempo_s"]))
    algos = [a for a in ("naive", "ikj", "blocked64") if a in times]
    names = {"naive": "naive (i-j-k)", "ikj": "ikj (orden de bucles)", "blocked64": "por bloques (64)"}
    short = {"naive": "naive", "ikj": "ikj", "blocked64": "bloques 64"}
    med = {a: {n: statistics.median(v) for n, v in times[a].items()} for a in algos}
    sizes = sorted({n for a in algos for n in med[a]})

    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 5.8))
    last_n = {x: max(med[x]) for x in algos}
    for i, alg in enumerate(algos):
        ns = sorted(med[alg])
        ts = [med[alg][n] for n in ns]
        a.plot(ns, ts, color=SERIES[i], label=names[alg])
        ring(a, ns, ts, SERIES[i], MARKERS[i], 7.5)
        rivals = [x for x in algos if x != alg and last_n[x] == ns[-1]]   # etiquetas finales que chocan
        dy = -3 if not rivals else (5 if ts[-1] >= max(med[x][ns[-1]] for x in rivals) else -11)
        a.annotate(f"{ts[-1]:.2f} s", (ns[-1], ts[-1]), textcoords="offset points", xytext=(8, dy),
                   fontsize=9, color=INK2)
    a.set_xscale("log", base=2)
    a.set_yscale("log")
    a.set_xticks(sizes)
    a.set_xticklabels([str(n) for n in sizes])
    a.set_xlabel("n (matrices n × n)")
    a.set_ylabel("Tiempo, mediana de 5 [s]")
    a.set_title("Tiempo de C = A·B (escala logarítmica)")
    a.legend(loc="upper left")
    width = 0.8 / len(algos)
    for i, alg in enumerate(algos):
        xs = [k + (i - (len(algos) - 1) / 2) * width for k, n in enumerate(sizes) if n in med[alg]]
        gf = [2 * n ** 3 / med[alg][n] * 1e-9 for n in sizes if n in med[alg]]
        b.bar(xs, gf, width=width - 0.04, color=SERIES[i], label=names[alg], edgecolor=SURFACE, linewidth=1.2)
    for k, n in enumerate(sizes):
        if "naive" in med and n not in med["naive"]:
            b.text(k - width, 0.15, "naive\nno se mide", ha="center", va="bottom", fontsize=8.5, color=MUTED)
    b.set_xticks(range(len(sizes)))
    b.set_xticklabels([str(n) for n in sizes])
    b.set_xlabel("n")
    b.set_ylabel("GFLOP/s  (2n³ / tiempo)")
    b.set_title("Rendimiento: lo que cambia es el uso de la caché")
    b.grid(axis="x", visible=False)
    title = "Línea base T(1) del producto denso"
    if "naive" in med and "ikj" in med and 1024 in med["naive"] and 1024 in med["ikj"]:
        title = (f"Línea base T(1): solo reordenar los bucles da {med['naive'][1024] / med['ikj'][1024]:.1f}× "
                 "en n = 1024, sin paralelismo")
    best = {n: min((med[x][n], x) for x in algos if n in med[x]) for n in sizes}
    best_txt = ", ".join(f"n = {n}: {short[x]}" for n, (t_, x) in best.items())
    suptitle(fig, title, f"T(1) = mejor versión secuencial por tamaño ({best_txt}); el speedup de S7 se calcula contra ella")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "06_t1_denso.png", f"Datos: {path.replace(os.sep, '/')} (make bench), -O2, mediana de 5 repeticiones")


def fig_t1_sim():
    path = latest("t1_sim_*.csv")
    if not path:
        return skip("07_t1_simulador.png", "make bench-sim")
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    g = defaultdict(list)
    for r in rows:
        g[(int(r["pisos"]), int(r["vanos"]), int(r["n"]), int(r["pasos"]), int(r["k"]))].append(r)
    keys = sorted(g, key=lambda k: k[2])
    fig, ax = plt.subplots(figsize=(11, 5.4))
    labels = []
    for i, key in enumerate(keys):
        pisos, vanos, n, pasos, k = key
        tt = statistics.median(float(r["tiempo_s"]) for r in g[key])
        tk = statistics.median(float(r["t_kernel_s"]) for r in g[key])
        fk = tk / tt
        ax.barh(i, fk * 100, height=0.5, color=SERIES[0], edgecolor=SURFACE, linewidth=1.5)
        ax.barh(i, (1 - fk) * 100, left=fk * 100, height=0.5, color=SERIES[1], edgecolor=SURFACE, linewidth=1.5)
        ax.text(fk * 50, i, f"K·U {fk * 100:.0f} %", ha="center", va="center", color="#ffffff", fontsize=10,
                fontweight="semibold")
        ax.text(fk * 100 + (1 - fk) * 50, i, f"resto {(1 - fk) * 100:.0f} %", ha="center", va="center",
                color=INK, fontsize=10)
        ax.text(101.5, i, f"máx. {1 / (1 - fk):.1f}×", ha="left", va="center", color=INK, fontsize=10.5,
                fontweight="semibold")
        per_step = tt / pasos
        step_txt = f"{per_step * 1e6:.1f} µs por paso" if per_step < 1e-4 else f"{per_step * 1e3:.2f} ms por paso"
        labels.append(f"{pisos} × {vanos}   (n = {n:,})".replace(",", " ") + "\n" + step_txt)
    ax.set_yticks(range(len(keys)))
    ax.set_yticklabels(labels)
    ax.invert_yaxis()
    ax.set_xlim(0, 112)
    ax.set_xticks([0, 25, 50, 75, 100])
    ax.set_xticklabels(["0 %", "25 %", "50 %", "75 %", "100 %"])
    ax.grid(axis="y", visible=False)
    ax.set_xlabel("Fracción del tiempo de simulación (mediana de 5)")
    ax.text(101.5, -0.75, "Amdahl si solo\nse paraleliza K·U", ha="left", va="center", color=INK2, fontsize=9)
    ax.plot([], [], linewidth=8, color=SERIES[0], label="Producto K·U (spmm_csr)")
    ax.plot([], [], linewidth=8, color=SERIES[1], label="Resto: actualizar U y picos de respuesta")
    ax.legend(loc="lower center", bbox_to_anchor=(0.45, 1.0), ncol=2)
    k0 = keys[0]
    suptitle(fig, "Simulador secuencial: K·U es solo la mitad del tiempo",
             f"{k0[3]} pasos, {k0[4]} escenarios. En S7 OpenMP debe paralelizar también la actualización de U, "
             "o el speedup no pasa de 1 / (1 − f)")
    fig.tight_layout(rect=(0, 0.02, 1, 0.9))
    save(fig, "07_t1_simulador.png", f"Datos: {path.replace(os.sep, '/')} (make bench-sim)")


def main():
    if not os.path.isdir(RES):
        sys.exit("Correr desde la raíz del repositorio (no se encuentra results/)")
    os.makedirs(OUT, exist_ok=True)
    setup_style()
    print(f"Figuras en {OUT}/")
    for f in (fig_validacion, fig_convergencia, fig_respuesta, fig_historia, fig_mapa, fig_t1_denso, fig_t1_sim):
        f()


if __name__ == "__main__":
    main()
