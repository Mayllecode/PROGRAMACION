/*
 * bench_sim.c — Mide T(1) del SIMULADOR completo con edificios cada vez más
 * grandes, y separa el tiempo del producto K·U del resto.
 *
 * Uso:  ./build/bench_sim [repeticiones] [pasos] [escenarios]
 *       ./build/bench_sim 5 400 8 > results/t1_sim_mipc.csv
 *
 * Por qué importa: si el producto K·U es una fracción f del tiempo y solo se
 * paraleliza esa parte, la ley de Amdahl limita el speedup a 1 / (1 − f),
 * por muchos hilos que haya. Este programa mide f para planificar S7.
 *
 * CSV (stdout), mismas primeras columnas que el protocolo de medición:
 *   algoritmo,n,repeticion,tiempo_s,gflops,pisos,vanos,k,pasos,t_kernel_s,frac_kernel
 * gflops = 2·nnz(K)·k·pasos / tiempo_s (ritmo efectivo de la simulación).
 * El resumen (medianas) sale por stderr.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "building.h"
#include "ground_motion.h"
#include "seq.h"
#include "sim.h"

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

int main(int argc, char **argv) {
    int reps = argc > 1 ? atoi(argv[1]) : 5;
    int steps = argc > 2 ? atoi(argv[2]) : 400;
    int k = argc > 3 ? atoi(argv[3]) : 8;
    if (reps < 1 || steps < 1 || k < 1) { fprintf(stderr, "repeticiones, pasos y escenarios >= 1\n"); return 1; }

    /* pisos x vanos (docs/PLAN_S5_S6.md, semana 6, tarea 4) */
    const int sizes[][2] = {{10, 4}, {20, 50}, {50, 200}, {100, 400}};
    const int n_sizes = (int)(sizeof sizes / sizeof sizes[0]);

    double *tt = malloc(reps * sizeof(double)), *tk = malloc(reps * sizeof(double));
    printf("algoritmo,n,repeticion,tiempo_s,gflops,pisos,vanos,k,pasos,t_kernel_s,frac_kernel\n");
    fprintf(stderr, "Simulador secuencial: %d pasos, %d escenarios, mediana de %d\n", steps, k, reps);
    fprintf(stderr, "%11s %8s %9s %11s %11s %9s %10s %12s\n",
            "pisos x vanos", "n", "nnz", "total_s", "K·U_s", "K·U %", "ms/paso", "Amdahl max");

    for (int z = 0; z < n_sizes; z++) {
        int pisos = sizes[z][0], vanos = sizes[z][1];
        Building *b = building_create(pisos, vanos, 2.0e4, 3.5e7, 1.0e8, 3.0);
        double T1 = building_fundamental_period(b);
        double dt = 0.5 * sim_dt_critical(b->K, b->mass);
        double alpha = 2.0 * 0.05 * (2.0 * 3.14159265358979323846 / T1);
        Matrix *ag = mat_alloc(steps, k);
        double *col = malloc(steps * sizeof(double));
        for (int s = 0; s < k; s++) {           /* k sismos sintéticos de 0.5 a 4 Hz */
            ground_motion_synthetic(col, steps, dt, 0.3 * G_ACCEL, 0.5 + 3.5 * s / (k > 1 ? k - 1 : 1));
            for (int t = 0; t < steps; t++) M_AT(ag, t, s) = col[t];
        }
        SimConfig cfg = {dt, steps, k, alpha, spmm_csr, -1};
        SimResult r;
        double flops = 2.0 * b->K->nnz * (double)k * steps;

        sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r); /* calentamiento */
        int finite = 1;
        for (size_t t = 0; t < (size_t)b->n * k; t++) if (!isfinite(r.u->data[t])) finite = 0;
        sim_result_free(&r);
        if (!finite) { fprintf(stderr, "ERROR: la simulacion %dx%d no es finita\n", pisos, vanos); return 1; }

        for (int rep = 0; rep < reps; rep++) {
            sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r);
            tt[rep] = r.t_total;
            tk[rep] = r.t_kernel;
            printf("sim_seq,%d,%d,%.6f,%.3f,%d,%d,%d,%d,%.6f,%.4f\n", b->n, rep + 1, r.t_total,
                   flops / r.t_total * 1e-9, pisos, vanos, k, steps, r.t_kernel, r.t_kernel / r.t_total);
            sim_result_free(&r);
        }
        double mt = median(tt, reps), mk = median(tk, reps), f = mk / mt;
        char label[32];
        snprintf(label, sizeof label, "%d x %d", pisos, vanos);
        fprintf(stderr, "%13s %8d %9d %11.4f %11.4f %8.0f%% %10.3f %11.1fx\n",
                label, b->n, b->K->nnz, mt, mk, 100.0 * f, 1e3 * mt / steps, 1.0 / (1.0 - f));
        mat_free(ag); free(col); building_free(b);
    }
    fprintf(stderr, "Amdahl max = 1/(1 - f): speedup maximo si SOLO se paraleliza K·U.\n");
    free(tt); free(tk);
    return 0;
}
