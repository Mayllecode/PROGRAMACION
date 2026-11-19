/*
 * demo_sismo.c — Simula un edificio ante varios sismos a la vez.
 *
 * Uso:
 *   ./build/demo_sismo [pisos] [vanos]
 *   ./build/demo_sismo 10 4 data/registro.txt 0.01 0.01 [columna]
 *                            ^archivo         ^dt  ^factor a m/s^2 (gal -> 0.01)
 *                                                       ^columna (0 = todos los números en orden)
 *
 * Por defecto corre 4 escenarios sintéticos de 0.3 g con distinta frecuencia
 * dominante (0.5, 1, 2 y 4 Hz). Si se da un registro real, se agrega como
 * quinto escenario y la duración se alarga hasta cubrirlo.
 *
 * Salidas en results/ (las lee scripts/graficos.py):
 *   respuesta.csv       desplazamiento y deriva máximos por piso y escenario
 *   mapa_deriva.csv     lo mismo por nodo (piso, columna): el mapa de demanda
 *   historia_techo.csv  a_g(t) y desplazamiento del techo en el tiempo
 *
 * Versión OpenMP (semana 7): el mismo archivo compilado con -fopenmp
 * (make demo-omp -> build/demo_sismo_omp) usa sim_run_omp + spmm_csr_omp.
 * El número de hilos se elige con OMP_NUM_THREADS. Los resultados son
 * idénticos a los de la versión secuencial; solo cambia el tiempo.
 *
 * Versión MPI (semana 11): compilado con -DUSE_MPI (make demo-mpi ->
 * build/demo_sismo_mpi) reparte los pisos del edificio entre procesos con
 * sim_run_mpi. Se corre con  mpiexec -n 4 ./build/demo_sismo_mpi 10 4
 * Solo el proceso 0 imprime y escribe los CSV; los demás solo calculan su
 * parte. El reparto es por no nulos; REPARTO=filas lo cambia a filas.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "building.h"
#include "ground_motion.h"
#include "seq.h"
#include "sim.h"
#ifdef _OPENMP
#include <omp.h>
#include "par_omp.h"
#endif
#ifdef USE_MPI
#include <mpi.h>
#include "par_mpi.h"
/* Si la raíz no puede seguir, hay que avisar a los demás procesos (que
 * esperan en sim_run_mpi): MPI_Abort los termina a todos. */
#define SALIR(code) do { MPI_Abort(MPI_COMM_WORLD, code); return code; } while (0)
#else
#define SALIR(code) return code
#endif

#define PI 3.14159265358979323846
#define N_SYNTH 4
#define HIST_MAX_ROWS 6000   /* filas de historia_techo.csv (se submuestrea si hay más pasos) */

/* Dibuja el pórtico en la consola: nodos (o), losas (-), columnas (|) y el
 * suelo. Si el edificio es muy grande se muestran solo los extremos (...). */
#define CELL 8
static void draw_building(const Building *b, double m, double kc, double kb) {
    int cols[8], nc = 0;
    if (b->vanos <= 8) { for (int c = 0; c <= b->vanos; c++) cols[nc++] = c; }
    else { cols[0] = 0; cols[1] = 1; cols[2] = 2; cols[3] = -1; cols[4] = b->vanos - 1; cols[5] = b->vanos; nc = 6; }
    int full_rows = b->pisos <= 12;
    char legend[4][80];
    snprintf(legend[0], 80, "o  = nodo (una masa de %.0f kg)", m);
    snprintf(legend[1], 80, "-  = losa/viga (resorte kb = %.1e N/m)", kb);
    snprintf(legend[2], 80, "|  = columna (resorte kc = %.1e N/m)", kc);
    snprintf(legend[3], 80, "%d nodos = %d pisos x %d columnas", b->n, b->pisos, b->vanos + 1);
    int li = 0;
    const int width = (nc - 1) * CELL + 1;
    char line[8 * CELL + 2];

    printf("\n%10s", "");
    for (int i = 0; i < nc; i++) {
        if (cols[i] < 0) printf("%-*s", CELL, "  ...");
        else { char lab[16]; snprintf(lab, sizeof lab, "col %d", cols[i]); printf("%-*s", CELL, lab); }
    }
    printf("\n");
    for (int p = b->pisos; p >= 1; p--) {
        if (!full_rows && p < b->pisos - 1 && p > 2) {          /* pisos del medio: se resumen */
            if (p == b->pisos - 2) {
                memset(line, ' ', width); line[width] = '\0';
                for (int i = 0; i < nc; i++) if (cols[i] >= 0) line[i * CELL] = ':';
                printf("  ...       %s   (pisos 3 a %d)\n", line, b->pisos - 2);
            }
            continue;
        }
        /* fila de nodos y losas */
        memset(line, ' ', width); line[width] = '\0';
        for (int i = 0; i < nc; i++) {
            line[i * CELL] = cols[i] < 0 ? '.' : 'o';
            if (i + 1 < nc)
                for (int t = 1; t < CELL; t++) line[i * CELL + t] = (cols[i] < 0 || cols[i + 1] < 0) ? ' ' : '-';
        }
        printf("piso %2d     %s", p, line);
        if (li < 4) printf("      %s", legend[li++]);
        printf("\n");
        /* fila de columnas hacia el piso de abajo (o el suelo) */
        memset(line, ' ', width); line[width] = '\0';
        for (int i = 0; i < nc; i++) line[i * CELL] = cols[i] < 0 ? ' ' : '|';
        printf("            %s", line);
        if (li < 4) printf("      %s", legend[li++]);
        printf("\n");
    }
    memset(line, '/', width + 4); line[width + 4] = '\0';
    printf("suelo     %s     (base fija: el suelo solo se sacude)\n", line);
    if (b->vanos <= 8 && b->vanos > 0) {
        printf("%14s", "");
        for (int v = 1; v <= b->vanos; v++) { char lab[16]; snprintf(lab, sizeof lab, "vano %d", v); printf("%-*s", CELL, lab); }
        printf("\n");
    }
    printf("Numeracion: nodo = (piso - 1) * %d + columna   ->   piso 1: nodos 0..%d, piso %d: nodos %d..%d\n\n",
           b->vanos + 1, b->vanos, b->pisos, b->n - b->vanos - 1, b->n - 1);
}

static FILE *open_out(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) fprintf(stderr, "Aviso: no se pudo escribir %s (existe la carpeta results/? make demo la crea)\n", path);
    return f;
}

int main(int argc, char **argv) {
#ifdef USE_MPI
    MPI_Init(&argc, &argv);
    int mpi_rank, mpi_p;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_p);
    const char *rep_env = getenv("REPARTO");
    const int part_mode = (rep_env && strcmp(rep_env, "filas") == 0) ? PART_ROWS : PART_NNZ;
    if (mpi_rank != 0) {   /* los demás procesos solo calculan su franja de pisos */
        SimMpiStats st;
        sim_run_mpi(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, MPI_COMM_WORLD, part_mode, &st);
        int info[3] = {st.rows_local, st.ghosts, st.neighbors};
        MPI_Gather(info, 3, MPI_INT, NULL, 3, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Finalize();
        return 0;
    }
#endif
    int pisos = argc > 1 ? atoi(argv[1]) : 10;
    int vanos = argc > 2 ? atoi(argv[2]) : 4;
    const char *rec_path = argc > 5 ? argv[3] : NULL;
    double rec_dt = argc > 5 ? atof(argv[4]) : 0.0, rec_factor = argc > 5 ? atof(argv[5]) : 1.0;
    int rec_col = argc > 6 ? atoi(argv[6]) : 0;
    if (pisos < 1 || vanos < 0) { fprintf(stderr, "pisos >= 1 y vanos >= 0\n"); SALIR(1); }
    if (argc == 4 || argc == 5) {
        fprintf(stderr, "Para un registro real: %s pisos vanos archivo dt factor [columna]\n", argv[0]);
        SALIR(1);
    }
    if (rec_path && (rec_dt <= 0.0 || rec_col < 0)) { fprintf(stderr, "dt del registro > 0 y columna >= 0\n"); SALIR(1); }

    /* Parámetros del modelo (ver docs/MODELO_FISICO.md) */
    const double m = 2.0e4, kc = 3.5e7, kb = 1.0e8, h = 3.0, zeta = 0.05;
    double duration = 30.0;
    Building *b = building_create(pisos, vanos, m, kc, kb, h);
    double T1 = building_fundamental_period(b), w1 = 2.0 * PI / T1;
    double alpha = 2.0 * zeta * w1;               /* 5 % de amortiguamiento en el modo 1 */
    double dtc = sim_dt_critical(b->K, b->mass), dt = 0.5 * dtc;

    /* Registro real: primero se lee para saber cuánto dura */
    RecordInfo ri;
    if (rec_path) {
        int got = ground_motion_load(rec_path, rec_col, rec_dt, rec_factor, dt, 0, NULL, &ri);
        if (got == -1) { fprintf(stderr, "No se pudo abrir %s\n", rec_path); SALIR(1); }
        if (got == -2) {
            if (ri.columns > 0)
                fprintf(stderr, "%s tiene %d columnas por linea: la columna %d no existe\n", rec_path, ri.columns, rec_col);
            else
                fprintf(stderr, "%s no tiene lineas de datos (solo cabecera o texto)\n", rec_path);
            SALIR(1);
        }
        duration = fmax(duration, got * rec_dt + 5.0);  /* + 5 s de vibración libre al final */
    }
    int N = (int)(duration / dt);

    const double freqs[N_SYNTH] = {0.5, 1.0, 2.0, 4.0};
    const char *ids[N_SYNTH + 1] = {"sint_0.5Hz", "sint_1.0Hz", "sint_2.0Hz", "sint_4.0Hz", "registro"};
    const char *labels[N_SYNTH + 1] = {"0.5 Hz", "1.0 Hz", "2.0 Hz", "4.0 Hz", "registro"};
    int k = N_SYNTH + (rec_path ? 1 : 0);
    Matrix *ag = mat_alloc(N, k);
    double *col = xcalloc((size_t)N, sizeof(double));
    for (int s = 0; s < N_SYNTH; s++) {
        ground_motion_synthetic(col, N, dt, 0.3 * G_ACCEL, freqs[s]);
        for (int t = 0; t < N; t++) M_AT(ag, t, s) = col[t];
    }

    printf("Edificio: %d pisos x %d vanos -> %d grados de libertad, nnz(K) = %d\n",
           pisos, vanos, b->n, b->K->nnz);
    draw_building(b, m, kc, kb);
    printf("Periodo fundamental: T1 = %.3f s (f1 = %.2f Hz)   |   dt = %.4f s (critico %.4f s)   |   %d pasos (%.0f s)\n",
           T1, 1.0 / T1, dt, dtc, N, duration);
    printf("Escenarios: %d sinteticos de 0.3 g (0.5, 1, 2 y 4 Hz)", N_SYNTH);
    if (rec_path) {
        ground_motion_load(rec_path, rec_col, rec_dt, rec_factor, dt, N, col, &ri);
        for (int t = 0; t < N; t++) M_AT(ag, t, N_SYNTH) = col[t];
        printf(" + registro real\n");
        char used[32];
        if (rec_col == 0) snprintf(used, sizeof used, "todos los valores en orden");
        else snprintf(used, sizeof used, "la columna %d", rec_col);
        printf("Registro %s: %d valor(es) por linea, se uso %s; %d muestras (%.1f s); %d lineas de cabecera ignoradas\n",
               rec_path, ri.columns, used, ri.samples, ri.samples * rec_dt, ri.header_lines);
        printf("PGA del registro = %.3f m/s^2 = %.3f g   (si sale absurda, revisar el factor de unidades)\n",
               ri.pga, ri.pga / G_ACCEL);
    } else {
        printf("\n");
    }
    printf("\n");

    const int roof = building_node(b, pisos, 0);
    SimResult r;
#if defined(USE_MPI)
    SimConfig cfg = {dt, N, k, alpha, spmm_csr, roof};
    SimMpiStats mst;
    int rc_mpi = sim_run_mpi(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r, MPI_COMM_WORLD, part_mode, &mst);
    {
        /* Cómo quedó repartido el edificio: la franja de pisos de cada proceso */
        int mine[3] = {mst.rows_local, mst.ghosts, mst.neighbors};
        int *info = xcalloc((size_t)mpi_p * 3, sizeof(int)), *starts = xcalloc((size_t)mpi_p + 1, sizeof(int));
        MPI_Gather(mine, 3, MPI_INT, info, 3, MPI_INT, 0, MPI_COMM_WORLD);
        partition_rows(b->K, mpi_p, part_mode, starts);
        const int W = vanos + 1;
        printf("Version MPI: %d procesos, reparto por %s (cambiar con REPARTO=filas o REPARTO=nnz)\n",
               mpi_p, part_mode == PART_NNZ ? "no nulos" : "filas");
        printf("  %-8s %-14s %-12s %6s %10s %8s\n", "proceso", "nodos", "pisos", "nnz", "fantasmas", "vecinos");
        for (int q = 0; q < mpi_p; q++) {
            char nodos[32], fl[32];
            if (starts[q + 1] > starts[q]) {
                snprintf(nodos, sizeof nodos, "%d..%d", starts[q], starts[q + 1] - 1);
                snprintf(fl, sizeof fl, "%d..%d", starts[q] / W + 1, (starts[q + 1] - 1) / W + 1);
            } else { snprintf(nodos, sizeof nodos, "(ninguno)"); snprintf(fl, sizeof fl, "-"); }
            printf("  %-8d %-14s %-12s %6d %10d %8d\n", q, nodos, fl,
                   b->K->row_ptr[starts[q + 1]] - b->K->row_ptr[starts[q]], info[3 * q + 1], info[3 * q + 2]);
        }
        int msgs_step = 0;
        for (int q = 0; q < mpi_p; q++) msgs_step += info[3 * q + 2];
        printf("  Desbalance de nnz: %.2f (1.00 = perfecto). En cada paso viajan %d mensajes de frontera.\n\n",
               partition_imbalance(b->K, mpi_p, starts), msgs_step);
        free(info); free(starts);
    }
    if (rc_mpi != 0) {
#elif defined(_OPENMP)
    printf("Version OpenMP: %d hilos (cambiar con OMP_NUM_THREADS)\n\n", omp_get_max_threads());
    SimConfig cfg = {dt, N, k, alpha, spmm_csr_omp, roof};
    if (sim_run_omp(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r) != 0) {
#else
    SimConfig cfg = {dt, N, k, alpha, spmm_csr, roof};
    if (sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r) != 0) {
#endif
        fprintf(stderr, "Datos inconsistentes\n");
        SALIR(1);
    }

    /* Máximos por piso (sobre todas las columnas de nodos del piso) */
    double *floor_u = xcalloc((size_t)pisos * k, sizeof(double));
    double *floor_d = xcalloc((size_t)pisos * k, sizeof(double));
    for (int p = 1; p <= pisos; p++)
        for (int s = 0; s < k; s++)
            for (int c = 0; c <= vanos; c++) {
                int i = building_node(b, p, c);
                floor_u[(p - 1) * k + s] = fmax(floor_u[(p - 1) * k + s], M_AT(r.peak_u, i, s));
                floor_d[(p - 1) * k + s] = fmax(floor_d[(p - 1) * k + s], M_AT(r.peak_drift, i, s) / h);
            }

    printf("Desplazamiento maximo por piso [cm]\n%5s", "piso");
    for (int s = 0; s < k; s++) printf("   %8s", labels[s]);
    printf("\n");
    for (int p = pisos; p >= 1; p--) {
        printf("%5d", p);
        for (int s = 0; s < k; s++) printf("   %8.2f", 100.0 * floor_u[(p - 1) * k + s]);
        printf("\n");
    }

    /* "Mapa" de deriva: el peor escenario en cada piso */
    double dmax = 0.0;
    for (int t = 0; t < pisos * k; t++) dmax = fmax(dmax, floor_d[t]);
    printf("\nDeriva maxima por piso (peor escenario), desplazamiento relativo / altura\n");
    for (int p = pisos; p >= 1; p--) {
        double d = 0.0;
        for (int s = 0; s < k; s++) d = fmax(d, floor_d[(p - 1) * k + s]);
        int bar = dmax > 0 ? (int)(40.0 * d / dmax + 0.5) : 0;
        printf("%5d  %.4f  ", p, d);
        for (int t = 0; t < bar; t++) putchar('#');
        putchar('\n');
    }

    /* Resumen por escenario: lo que se reporta de cada sismo */
    printf("\nResumen por escenario\n  %-10s %18s %12s %13s\n", "escenario", "desp. techo [cm]", "deriva max", "piso critico");
    int s_top = 0, s_second = -1;
    for (int s = 0; s < k; s++) {
        int p_crit = 1;
        for (int p = 1; p <= pisos; p++)
            if (floor_d[(p - 1) * k + s] > floor_d[(p_crit - 1) * k + s]) p_crit = p;
        printf("  %-10s %18.2f %12.4f %13d\n", labels[s], 100.0 * floor_u[(pisos - 1) * k + s],
               floor_d[(p_crit - 1) * k + s], p_crit);
    }
    for (int s = 1; s < N_SYNTH; s++)
        if (floor_u[(pisos - 1) * k + s] > floor_u[(pisos - 1) * k + s_top]) s_top = s;
    for (int s = 0; s < N_SYNTH; s++)
        if (s != s_top && (s_second < 0 || floor_u[(pisos - 1) * k + s] > floor_u[(pisos - 1) * k + s_second])) s_second = s;
    int s_near = 0;
    for (int s = 1; s < N_SYNTH; s++)
        if (fabs(freqs[s] - 1.0 / T1) < fabs(freqs[s_near] - 1.0 / T1)) s_near = s;
    if (s_top == s_near && s_second >= 0 && floor_u[(pisos - 1) * k + s_second] > 0.0)
        printf("\nResonancia: el sismo de %.1f Hz es el mas cercano a f1 = %.2f Hz y mueve el techo\n"
               "%.1f veces mas que el siguiente escenario sintetico (%s).\n",
               freqs[s_top], 1.0 / T1,
               floor_u[(pisos - 1) * k + s_top] / floor_u[(pisos - 1) * k + s_second], labels[s_second]);

    /* ---- Archivos para los gráficos ---- */
    FILE *f = open_out("results/respuesta.csv");
    if (f) {
        fprintf(f, "escenario,piso,desp_max_m,deriva_max\n");
        for (int s = 0; s < k; s++)
            for (int p = 1; p <= pisos; p++)
                fprintf(f, "%s,%d,%.6e,%.6e\n", ids[s], p, floor_u[(p - 1) * k + s], floor_d[(p - 1) * k + s]);
        fclose(f);
    }
    f = open_out("results/mapa_deriva.csv");
    if (f) {
        fprintf(f, "escenario,piso,columna,nodo,desp_max_m,deriva_max\n");
        for (int s = 0; s < k; s++)
            for (int p = 1; p <= pisos; p++)
                for (int c = 0; c <= vanos; c++) {
                    int i = building_node(b, p, c);
                    fprintf(f, "%s,%d,%d,%d,%.6e,%.6e\n", ids[s], p, c, i, M_AT(r.peak_u, i, s),
                            M_AT(r.peak_drift, i, s) / h);
                }
        fclose(f);
    }
    f = open_out("results/historia_techo.csv");
    if (f) {
        int stride = (N + HIST_MAX_ROWS - 1) / HIST_MAX_ROWS;
        fprintf(f, "t_s");
        for (int s = 0; s < k; s++) fprintf(f, ",ag_%s", ids[s]);
        for (int s = 0; s < k; s++) fprintf(f, ",u_techo_%s", ids[s]);
        fprintf(f, "\n");
        for (int t = 0; t < N; t += stride) {
            fprintf(f, "%.5f", t * dt);
            for (int s = 0; s < k; s++) fprintf(f, ",%.6e", M_AT(ag, t, s));
            for (int s = 0; s < k; s++) fprintf(f, ",%.6e", r.history[(size_t)t * k + s]);
            fprintf(f, "\n");
        }
        fclose(f);
    }

    printf("\nTiempo total: %.3f s   |   dentro de K*U: %.3f s (%.0f %%)   |   %d productos K*U\n",
           r.t_total, r.t_kernel, 100.0 * r.t_kernel / r.t_total, N);
    printf("Resultados en results/respuesta.csv, results/mapa_deriva.csv y results/historia_techo.csv\n");

#ifdef USE_MPI
    printf("Comunicacion: %.3f s intercambiando fronteras (el mas lento de los %d procesos)\n",
           mst.s.t_comm, mpi_p);
#endif

    sim_result_free(&r);
    mat_free(ag); free(col); free(floor_u); free(floor_d); building_free(b);
#ifdef USE_MPI
    MPI_Finalize();
#endif
    return 0;
}
