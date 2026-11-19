/*
 * bench_sim_omp.c — Speedup del SIMULADOR sísmico con OpenMP (semana 7).
 *
 * Uso:  ./build/bench_sim_omp [repeticiones] [pasos] [escenarios] [hilos]
 *       ./build/bench_sim_omp 5 400 8 1,2,4,8 > results/omp_sim_mipc.csv
 *
 * Compara estas versiones con los mismos edificios y sismos:
 *   sim_seq          sim_run + spmm_csr          \  T(1) = la más rápida
 *   sim_seq_rowinit  sim_run + spmm_csr_rowinit  /  de las dos secuenciales
 *   sim_omp_soloKU   sim_run + spmm_csr_omp      -> solo el producto en paralelo
 *   sim_omp          sim_run_omp + spmm_csr_omp  -> todo el paso en paralelo
 *
 * La segunda muestra la ley de Amdahl en la práctica: con una fracción f
 * del tiempo en K·U, su speedup no puede pasar de 1 / ((1 − f) + f/p).
 * El resumen imprime ese límite al lado del valor medido.
 *
 * Antes de medir, cada versión paralela se VERIFICA: su desplazamiento
 * final tiene que ser idéntico bit a bit al secuencial.
 *
 * CSV (stdout):
 *   algoritmo,n,repeticion,tiempo_s,gflops,p,pisos,vanos,k,pasos,t_kernel_s,frac_kernel
 */
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "building.h"
#include "ground_motion.h"
#include "par_omp.h"
#include "seq.h"
#include "sim.h"
#include "verify.h"

#define PI 3.14159265358979323846
#define MAX_P 64

typedef int (*sim_fn)(const CSR *, const double *, const int *, const SimConfig *,
                      const Matrix *, const Matrix *, const Matrix *, SimResult *);

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(const double *v, int n) {
    double *c = malloc(n * sizeof(double));
    memcpy(c, v, n * sizeof(double));
    qsort(c, n, sizeof(double), cmp_double);
    double m = (n % 2) ? c[n / 2] : 0.5 * (c[n / 2 - 1] + c[n / 2]);
    free(c);
    return m;
}

static int parse_threads(const char *s, int *out) {
    int n = 0;
    while (*s && n < MAX_P) {
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s || v < 1) return 0;
        out[n++] = (int)v;
        s = (*end == ',') ? end + 1 : end;
        if (*end && *end != ',') return 0;
    }
    return n;
}

int main(int argc, char **argv) {
    int reps = argc > 1 ? atoi(argv[1]) : 5;
    int steps = argc > 2 ? atoi(argv[2]) : 400;
    int k = argc > 3 ? atoi(argv[3]) : 8;
    int threads[MAX_P] = {1, 2, 4, 8}, n_threads = 4;
    if (argc > 4 && !(n_threads = parse_threads(argv[4], threads))) {
        fprintf(stderr, "hilos: lista separada por comas, por ejemplo 1,2,4,8\n");
        return 1;
    }
    if (reps < 1 || steps < 1 || k < 1) { fprintf(stderr, "repeticiones, pasos y escenarios >= 1\n"); return 1; }
    omp_set_dynamic(0);

    /* pisos x vanos: uno chico (para ver el costo de crear hilos) y dos grandes */
    const int sizes[][2] = {{20, 50}, {50, 200}, {100, 400}};
    const int n_sizes = (int)(sizeof sizes / sizeof sizes[0]);

    double *tt = malloc(reps * sizeof(double)), *tk = malloc(reps * sizeof(double));
    printf("algoritmo,n,repeticion,tiempo_s,gflops,p,pisos,vanos,k,pasos,t_kernel_s,frac_kernel\n");
    fprintf(stderr, "Simulador con OpenMP: %d pasos, %d escenarios, mediana de %d; %d nucleos logicos\n",
            steps, k, reps, omp_get_num_procs());

    for (int z = 0; z < n_sizes; z++) {
        const int pisos = sizes[z][0], vanos = sizes[z][1];
        Building *b = building_create(pisos, vanos, 2.0e4, 3.5e7, 1.0e8, 3.0);
        const double T1b = building_fundamental_period(b);
        const double dt = 0.5 * sim_dt_critical(b->K, b->mass);
        const double alpha = 2.0 * 0.05 * (2.0 * PI / T1b);
        Matrix *ag = mat_alloc(steps, k);
        double *col = malloc(steps * sizeof(double));
        for (int s = 0; s < k; s++) {           /* k sismos sintéticos de 0.5 a 4 Hz (como bench_sim) */
            ground_motion_synthetic(col, steps, dt, 0.3 * G_ACCEL, 0.5 + 3.5 * s / (k > 1 ? k - 1 : 1));
            for (int t = 0; t < steps; t++) M_AT(ag, t, s) = col[t];
        }
        const double flops = 2.0 * b->K->nnz * (double)k * steps;
        SimResult r, ref;

        /* ---- T(1): el mejor simulador secuencial (K·U con spmm_csr o spmm_csr_rowinit) ---- */
        const struct { const char *name; spmm_fn f; } seqs[] = {
            {"sim_seq", spmm_csr}, {"sim_seq_rowinit", spmm_csr_rowinit},
        };
        SimConfig cfg_ref = {dt, steps, k, alpha, spmm_csr, -1};
        sim_run(b->K, b->mass, b->below, &cfg_ref, ag, NULL, NULL, &ref);   /* calentamiento y referencia */
        double T1 = 0.0, f = 0.0;
        const char *best = "";
        for (int a = 0; a < 2; a++) {
            SimConfig cfg_seq = {dt, steps, k, alpha, seqs[a].f, -1};
            for (int rep = 0; rep < reps; rep++) {
                sim_run(b->K, b->mass, b->below, &cfg_seq, ag, NULL, NULL, &r);
                tt[rep] = r.t_total; tk[rep] = r.t_kernel;
                printf("%s,%d,%d,%.6f,%.3f,1,%d,%d,%d,%d,%.6f,%.4f\n", seqs[a].name, b->n, rep + 1, r.t_total,
                       flops / r.t_total * 1e-9, pisos, vanos, k, steps, r.t_kernel, r.t_kernel / r.t_total);
                sim_result_free(&r);
            }
            const double med = median(tt, reps);
            if (a == 0 || med < T1) { T1 = med; f = median(tk, reps) / med; best = seqs[a].name; }
        }
        fprintf(stderr, "\nEdificio %d x %d (n = %d, nnz = %d): T(1) = %.4f s (%s), K*U = %.0f %% del tiempo\n",
                pisos, vanos, b->n, b->K->nnz, T1, best, 100.0 * f);
        fprintf(stderr, "  %-15s %3s %11s %7s %7s %13s\n", "version", "p", "mediana_s", "S(p)", "E(p)", "Amdahl max");

        const struct { const char *name; sim_fn run; } vers[] = {
            {"sim_omp_soloKU", sim_run}, {"sim_omp", sim_run_omp},
        };
        SimConfig cfg_omp = {dt, steps, k, alpha, spmm_csr_omp, -1};
        for (int v = 0; v < 2; v++)
            for (int h = 0; h < n_threads; h++) {
                const int p = threads[h];
                omp_set_num_threads(p);
                vers[v].run(b->K, b->mass, b->below, &cfg_omp, ag, NULL, NULL, &r);   /* calentamiento */
                int same = mat_equal_exact(ref.u, r.u) && mat_equal_exact(ref.peak_drift, r.peak_drift);
                sim_result_free(&r);
                if (!same) {
                    fprintf(stderr, "ERROR: %s p=%d no da el mismo resultado que el secuencial\n", vers[v].name, p);
                    return 1;
                }
                for (int rep = 0; rep < reps; rep++) {
                    vers[v].run(b->K, b->mass, b->below, &cfg_omp, ag, NULL, NULL, &r);
                    tt[rep] = r.t_total; tk[rep] = r.t_kernel;
                    printf("%s,%d,%d,%.6f,%.3f,%d,%d,%d,%d,%d,%.6f,%.4f\n", vers[v].name, b->n, rep + 1,
                           r.t_total, flops / r.t_total * 1e-9, p, pisos, vanos, k, steps, r.t_kernel,
                           r.t_kernel / r.t_total);
                    sim_result_free(&r);
                }
                const double med = median(tt, reps), S = T1 / med;
                if (v == 0)   /* límite de Amdahl si solo K·U va en paralelo */
                    fprintf(stderr, "  %-15s %3d %11.4f %7.2f %7.2f %12.2fx\n", vers[v].name, p, med, S, S / p,
                            1.0 / ((1.0 - f) + f / p));
                else
                    fprintf(stderr, "  %-15s %3d %11.4f %7.2f %7.2f %13s\n", vers[v].name, p, med, S, S / p, "-");
            }
        sim_result_free(&ref);
        mat_free(ag); free(col); building_free(b);
    }
    fprintf(stderr, "\nsim_omp_soloKU: solo K*U en paralelo; nunca pasa el limite de Amdahl 1/((1-f) + f/p).\n"
                    "sim_omp: K*U + actualizacion de U + picos en paralelo.\n");
    free(tt); free(tk);
    return 0;
}
