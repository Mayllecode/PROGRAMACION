/*
 * bench_omp.c — Speedup y eficiencia del producto de matrices con OpenMP
 * (entregable de la semana 7: S(p) y E(p) con 1, 2, 4 y 8 hilos).
 *
 * Uso:  ./build/bench_omp [repeticiones] [hilos] [n1 n2 ...]
 *       ./build/bench_omp 5 1,2,4,8 256 512 1024 2048 > results/omp_mipc.csv
 *
 * Protocolo (docs/PROTOCOLO_MEDICION.md):
 *   - T(1) es la MEJOR versión secuencial (ikj o bloques de 64, la que salga
 *     más rápida en esta máquina), medida en esta misma corrida y con las
 *     mismas banderas de compilación. No es la versión OpenMP con 1 hilo.
 *   - 1 ejecución de calentamiento por caso, que además se VERIFICA contra
 *     la referencia; si no coincide, el programa se detiene sin medir.
 *   - `repeticiones` ejecuciones medidas; se reporta la mediana.
 *   - S(p) = T(1) / T(p),   E(p) = S(p) / p.
 *
 * CSV (stdout): algoritmo,n,repeticion,tiempo_s,gflops,p
 * El resumen con S(p) y E(p) sale por stderr para no ensuciar el CSV.
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generators.h"
#include "matrix.h"
#include "par_omp.h"
#include "seq.h"
#include "timer.h"
#include "verify.h"

#define BLOCK 64
#define SPMM_K 16
#define MAX_P 64

typedef int (*matmul_fn)(const Matrix *, const Matrix *, Matrix *);
static int blocked(const Matrix *A, const Matrix *B, Matrix *C)     { return matmul_blocked(A, B, C, BLOCK); }
static int omp_blocked(const Matrix *A, const Matrix *B, Matrix *C) { return matmul_omp_blocked(A, B, C, BLOCK); }

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

/* "1,2,4,8" -> {1, 2, 4, 8}; devuelve cuántos leyó (0 si hay un error) */
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

/* Mide `reps` veces C = A·B con f, imprime cada tiempo en CSV y devuelve la mediana */
static double time_dense(matmul_fn f, const char *name, const Matrix *A, const Matrix *B, Matrix *C,
                         int reps, int p, double *t) {
    const int n = A->rows;
    const double flops = 2.0 * n * (double)n * n;
    for (int r = 0; r < reps; r++) {
        double t0 = now_sec();
        f(A, B, C);
        t[r] = now_sec() - t0;
        printf("%s,%d,%d,%.6f,%.3f,%d\n", name, n, r + 1, t[r], flops / t[r] * 1e-9, p);
    }
    return median(t, reps);
}

int main(int argc, char **argv) {
    int reps = argc > 1 ? atoi(argv[1]) : 5;
    int threads[MAX_P] = {1, 2, 4, 8}, n_threads = 4;
    if (argc > 2 && !(n_threads = parse_threads(argv[2], threads))) {
        fprintf(stderr, "hilos: lista separada por comas, por ejemplo 1,2,4,8\n");
        return 1;
    }
    int default_sizes[] = {256, 512, 1024, 2048};
    int *sizes = default_sizes, n_sizes = 4;
    if (argc > 3) {
        n_sizes = argc - 3;
        sizes = malloc(n_sizes * sizeof(int));
        for (int s = 0; s < n_sizes; s++) sizes[s] = atoi(argv[s + 3]);
    }
    if (reps < 1) { fprintf(stderr, "repeticiones debe ser >= 1\n"); return 1; }
    omp_set_dynamic(0);

    double *t = malloc(reps * sizeof(double));
    printf("algoritmo,n,repeticion,tiempo_s,gflops,p\n");
    fprintf(stderr, "OpenMP: %d nucleos logicos disponibles; mediana de %d repeticiones\n",
            omp_get_num_procs(), reps);

    /* ---- Denso: C = A·B, n x n ---- */
    for (int s = 0; s < n_sizes; s++) {
        const int n = sizes[s];
        const double flops = 2.0 * n * (double)n * n;
        Matrix *A = gen_random(n, n, 1), *B = gen_random(n, n, 2);
        Matrix *R = mat_alloc(n, n), *C = mat_alloc(n, n);
        matmul_ikj(A, B, R);   /* referencia (idéntica a naive, ver test_seq) */

        /* T(1): la mejor secuencial */
        const struct { const char *name; matmul_fn f; } seqs[] = {{"ikj", matmul_ikj}, {"blocked64", blocked}};
        double T1 = 0.0;
        const char *best = "";
        for (int a = 0; a < 2; a++) {
            seqs[a].f(A, B, C);
            double med = time_dense(seqs[a].f, seqs[a].name, A, B, C, reps, 1, t);
            if (a == 0 || med < T1) { T1 = med; best = seqs[a].name; }
        }
        fprintf(stderr, "\nDenso n = %d: T(1) = %.4f s (mejor secuencial: %s)\n", n, T1, best);
        fprintf(stderr, "  %-14s %3s %11s %9s %7s %7s\n", "algoritmo", "p", "mediana_s", "GFLOP/s", "S(p)", "E(p)");

        const struct { const char *name; matmul_fn f; } pars[] = {{"omp_ikj", matmul_omp_ikj}, {"omp_blocked64", omp_blocked}};
        for (int a = 0; a < 2; a++)
            for (int h = 0; h < n_threads; h++) {
                const int p = threads[h];
                omp_set_num_threads(p);
                pars[a].f(A, B, C);   /* calentamiento + verificación */
                if (!mat_compare(R, C, VERIFY_EPS).ok) {
                    fprintf(stderr, "ERROR: %s n=%d p=%d no pasa la verificacion\n", pars[a].name, n, p);
                    return 1;
                }
                double med = time_dense(pars[a].f, pars[a].name, A, B, C, reps, p, t);
                fprintf(stderr, "  %-14s %3d %11.4f %9.3f %7.2f %7.2f\n", pars[a].name, p, med,
                        flops / med * 1e-9, T1 / med, T1 / med / p);
            }
        mat_free(A); mat_free(B); mat_free(R); mat_free(C);
    }

    /* ---- Disperso: K (rigidez 2D, lado x lado nodos) · U (n x SPMM_K) ---- */
    const int sides[] = {128, 256, 512};
    for (int s = 0; s < 3; s++) {
        const int side = sides[s], n = side * side;
        CSR *K = gen_stiffness_2d(side, side);
        Matrix *U = gen_random(n, SPMM_K, 3), *R = mat_alloc(n, SPMM_K), *C = mat_alloc(n, SPMM_K);
        const double flops = 2.0 * K->nnz * (double)SPMM_K;

        spmm_csr(K, U, R);   /* referencia */

        /* T(1): la mejor secuencial (spmm_csr o spmm_csr_rowinit) */
        const struct { const char *name; int (*f)(const CSR *, const Matrix *, Matrix *); } seqs[] = {
            {"spmm_csr", spmm_csr}, {"spmm_csr_rowinit", spmm_csr_rowinit},
        };
        double T1 = 0.0;
        const char *best = "";
        for (int a = 0; a < 2; a++) {
            seqs[a].f(K, U, C);   /* calentamiento */
            for (int r = 0; r < reps; r++) {
                double t0 = now_sec();
                seqs[a].f(K, U, C);
                t[r] = now_sec() - t0;
                printf("%s_k%d,%d,%d,%.6f,%.3f,1\n", seqs[a].name, SPMM_K, n, r + 1, t[r], flops / t[r] * 1e-9);
            }
            const double med = median(t, reps);
            if (a == 0 || med < T1) { T1 = med; best = seqs[a].name; }
        }
        fprintf(stderr, "\nDisperso K*U, malla %dx%d (n = %d, nnz = %d, %d escenarios): T(1) = %.5f s (%s)\n",
                side, side, n, K->nnz, SPMM_K, T1, best);
        fprintf(stderr, "  %-14s %3s %11s %9s %7s %7s\n", "algoritmo", "p", "mediana_s", "GFLOP/s", "S(p)", "E(p)");

        for (int h = 0; h < n_threads; h++) {
            const int p = threads[h];
            omp_set_num_threads(p);
            spmm_csr_omp(K, U, C);
            if (!mat_compare(R, C, VERIFY_EPS).ok) {
                fprintf(stderr, "ERROR: spmm_csr_omp n=%d p=%d no pasa la verificacion\n", n, p);
                return 1;
            }
            for (int r = 0; r < reps; r++) {
                double t0 = now_sec();
                spmm_csr_omp(K, U, C);
                t[r] = now_sec() - t0;
                printf("spmm_csr_omp_k%d,%d,%d,%.6f,%.3f,%d\n", SPMM_K, n, r + 1, t[r], flops / t[r] * 1e-9, p);
            }
            const double med = median(t, reps);
            fprintf(stderr, "  %-14s %3d %11.5f %9.3f %7.2f %7.2f\n", "spmm_csr_omp", p, med,
                    flops / med * 1e-9, T1 / med, T1 / med / p);
        }
        csr_free(K); mat_free(U); mat_free(R); mat_free(C);
    }

    fprintf(stderr, "\nS(p) = T(1)/T(p) con T(1) = mejor secuencial;  E(p) = S(p)/p.\n");
    free(t);
    if (sizes != default_sizes) free(sizes);
    return 0;
}
