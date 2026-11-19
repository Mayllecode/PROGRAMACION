/*
 * bench_seq.c — Mide T(1): la línea base contra la que se calculan
 * S(p) = T(1)/T(p) y E(p) = S(p)/p en todas las fases siguientes.
 *
 * Uso:  ./build/bench_seq [repeticiones] [n1 n2 ...]
 *       ./build/bench_seq 5 256 512 1024 > results/t1_mipc.csv
 *
 * Protocolo (ver docs/PROTOCOLO_MEDICION.md):
 *   - 1 ejecución de calentamiento por caso, no medida, que además
 *     se VERIFICA contra la referencia antes de medir nada.
 *   - `repeticiones` ejecuciones medidas; se reporta cada una en CSV.
 *   - El resumen (mediana y GFLOP/s) sale por stderr para no ensuciar el CSV.
 *
 * La versión naive solo se mide hasta n = 1024 (más arriba tarda demasiado).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generators.h"
#include "matrix.h"
#include "seq.h"
#include "timer.h"
#include "verify.h"

#define BLOCK 64
#define NAIVE_MAX_N 1024
#define SPMM_K 16

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(double *v, int n) {
    double *c = malloc(n * sizeof(double));
    memcpy(c, v, n * sizeof(double));
    qsort(c, n, sizeof(double), cmp_double);
    double m = (n % 2) ? c[n / 2] : 0.5 * (c[n / 2 - 1] + c[n / 2]);
    free(c);
    return m;
}

static int blocked(const Matrix *A, const Matrix *B, Matrix *C) { return matmul_blocked(A, B, C, BLOCK); }

int main(int argc, char **argv) {
    int reps = 5;
    int default_sizes[] = {256, 512, 1024};
    int *sizes = default_sizes, n_sizes = 3;
    if (argc > 1) reps = atoi(argv[1]);
    if (argc > 2) {
        n_sizes = argc - 2;
        sizes = malloc(n_sizes * sizeof(int));
        for (int s = 0; s < n_sizes; s++) sizes[s] = atoi(argv[s + 2]);
    }
    if (reps < 1) { fprintf(stderr, "repeticiones debe ser >= 1\n"); return 1; }

    const struct { const char *name; int (*f)(const Matrix *, const Matrix *, Matrix *); } algos[] = {
        {"naive", matmul_naive}, {"ikj", matmul_ikj}, {"blocked64", blocked},
    };

    double *t = malloc(reps * sizeof(double));
    printf("algoritmo,n,repeticion,tiempo_s,gflops\n");
    fprintf(stderr, "%-12s %8s %12s %10s\n", "algoritmo", "n", "mediana_s", "GFLOP/s");

    /* ---- Denso: C = A·B, n x n ---- */
    for (int s = 0; s < n_sizes; s++) {
        int n = sizes[s];
        Matrix *A = gen_random(n, n, 1), *B = gen_random(n, n, 2);
        Matrix *R = mat_alloc(n, n), *C = mat_alloc(n, n);
        matmul_ikj(A, B, R); /* referencia rápida (idéntica a naive, ver tests) */
        double flops = 2.0 * n * (double)n * n;

        for (int a = 0; a < 3; a++) {
            if (a == 0 && n > NAIVE_MAX_N) continue;
            algos[a].f(A, B, C); /* calentamiento + verificación */
            VerifyResult vr = mat_compare(R, C, VERIFY_EPS);
            if (!vr.ok) { fprintf(stderr, "ERROR: %s n=%d no pasa la verificación\n", algos[a].name, n); return 1; }
            for (int r = 0; r < reps; r++) {
                double t0 = now_sec();
                algos[a].f(A, B, C);
                t[r] = now_sec() - t0;
                printf("%s,%d,%d,%.6f,%.3f\n", algos[a].name, n, r + 1, t[r], flops / t[r] * 1e-9);
            }
            double med = median(t, reps);
            fprintf(stderr, "%-12s %8d %12.4f %10.3f\n", algos[a].name, n, med, flops / med * 1e-9);
        }
        mat_free(A); mat_free(B); mat_free(R); mat_free(C);
    }

    /* ---- Disperso: K (rigidez 2D, lado x lado nodos) · B (n x SPMM_K) ---- */
    const int sides[] = {128, 256, 512};
    for (int s = 0; s < 3; s++) {
        int side = sides[s], n = side * side;
        CSR *K = gen_stiffness_2d(side, side);
        Matrix *B = gen_random(n, SPMM_K, 3), *C = mat_alloc(n, SPMM_K);
        double flops = 2.0 * K->nnz * (double)SPMM_K;
        spmm_csr(K, B, C); /* calentamiento */
        for (int r = 0; r < reps; r++) {
            double t0 = now_sec();
            spmm_csr(K, B, C);
            t[r] = now_sec() - t0;
            printf("spmm_rigidez_k%d,%d,%d,%.6f,%.3f\n", SPMM_K, n, r + 1, t[r], flops / t[r] * 1e-9);
        }
        double med = median(t, reps);
        fprintf(stderr, "%-12s %8d %12.4f %10.3f   (nnz=%d)\n", "spmm_K", n, med, flops / med * 1e-9, K->nnz);
        csr_free(K); mat_free(B); mat_free(C);
    }

    free(t);
    if (sizes != default_sizes) free(sizes);
    return 0;
}
