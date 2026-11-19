/*
 * bench_mpi.c — Tiempo y comunicación de Cannon y SUMMA (semanas 9 y 10).
 *
 * Uso (una corrida por cada número de procesos; make bench-mpi las hace todas):
 *     mpiexec -n 1 ./build/bench_mpi 5 512,1024,2048 >  results/mpi_mipc.csv
 *     mpiexec -n 4 ./build/bench_mpi 5 512,1024,2048 -noheader >> results/mpi_mipc.csv
 *
 * Con p = 1 mide además T(1): la mejor versión secuencial (ikj o bloques 64),
 * igual que en S7. scripts/graficos_s11.py calcula S(p) = T(1)/T(p) y E(p).
 *
 * Antes de medir, cada algoritmo se VERIFICA contra matmul_ikj (dentro de ε):
 * si no coincide, el programa se detiene sin medir.
 *
 * Qué se mide (el máximo entre procesos; termina cuando termina el más lento):
 *   tiempo_s   la llamada completa: repartir A y B, calcular, juntar C
 *   t_comm_s   comunicación DURANTE el cálculo (rotaciones o difusiones)
 *   t_comp_s   productos locales
 *   t_dist_s   repartir desde la raíz y juntar en la raíz
 *   mensajes, bytes: recibidos durante el cálculo, sumados entre procesos
 *
 * CSV (stdout):
 *   algoritmo,n,p,malla,repeticion,tiempo_s,t_comm_s,t_comp_s,t_dist_s,mensajes,bytes,gflops
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generators.h"
#include "par_mpi.h"
#include "seq.h"
#include "timer.h"
#include "verify.h"

#define MAX_N 16

static int parse_list(const char *s, int *out) {
    int n = 0;
    while (*s && n < MAX_N) {
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s || v < 1) return 0;
        out[n++] = (int)v;
        if (*end && *end != ',') return 0;
        s = (*end == ',') ? end + 1 : end;
    }
    return n;
}

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

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, p;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    int reps = argc > 1 ? atoi(argv[1]) : 5;
    int sizes[MAX_N] = {512, 1024, 2048}, n_sizes = 3;
    if (argc > 2 && !(n_sizes = parse_list(argv[2], sizes))) {
        if (rank == 0) fprintf(stderr, "tamanos: lista separada por comas, por ejemplo 512,1024\n");
        MPI_Finalize();
        return 1;
    }
    int header = !(argc > 3 && strcmp(argv[3], "-noheader") == 0);
    if (reps < 1) reps = 1;

    const int q = mpi_square_side(p);
    int pr, pc;
    mpi_grid_dims(p, &pr, &pc);
    if (rank == 0) {
        if (header) printf("algoritmo,n,p,malla,repeticion,tiempo_s,t_comm_s,t_comp_s,t_dist_s,mensajes,bytes,gflops\n");
        fprintf(stderr, "\nProducto denso con MPI: p = %d procesos, mediana de %d\n", p, reps);
        fprintf(stderr, "  %-8s %5s %6s %10s %9s %9s %9s %9s %10s\n", "algoritmo", "n", "malla", "mediana_s",
                "comm_s", "calc_s", "reparto_s", "mensajes", "MB");
    }
    double *tt = malloc(reps * sizeof(double));

    for (int z = 0; z < n_sizes; z++) {
        const int n = sizes[z];
        const double flops = 2.0 * n * (double)n * n;
        Matrix *A = NULL, *B = NULL, *C = NULL, *R = NULL;
        if (rank == 0) {
            A = gen_random(n, n, 1000 + n); B = gen_random(n, n, 2000 + n);
            C = mat_alloc(n, n); R = mat_alloc(n, n);
            matmul_ikj(A, B, R);   /* referencia para verificar */
        }

        /* ---- T(1): la mejor secuencial, solo cuando p = 1 ---- */
        if (p == 1) {
            const char *names[] = {"seq_ikj", "seq_blocked64"};
            for (int a = 0; a < 2; a++) {
                for (int rep = 0; rep < reps; rep++) {
                    double t0 = now_sec();
                    if (a == 0) matmul_ikj(A, B, C); else matmul_blocked(A, B, C, 64);
                    tt[rep] = now_sec() - t0;
                    printf("%s,%d,1,1x1,%d,%.6f,0,%.6f,0,0,0,%.3f\n", names[a], n, rep + 1, tt[rep], tt[rep],
                           flops / tt[rep] * 1e-9);
                }
                fprintf(stderr, "  %-8s %5d %6s %10.4f\n", names[a], n, "-", median(tt, reps));
            }
            fflush(stdout);
        }

        const struct { const char *name; int (*f)(const Matrix *, const Matrix *, Matrix *, MPI_Comm, MpiStats *); int ok; } algs[] = {
            {"cannon", matmul_cannon, q > 0}, {"summa", matmul_summa, 1},
        };
        for (int a = 0; a < 2; a++) {
            if (!algs[a].ok) continue;
            char grid[16];
            if (a == 0) snprintf(grid, sizeof grid, "%dx%d", q, q); else snprintf(grid, sizeof grid, "%dx%d", pr, pc);
            /* verificación + calentamiento */
            MpiStats st, tot;
            algs[a].f(A, B, C, MPI_COMM_WORLD, &st);
            int ok = 1;
            if (rank == 0) ok = mat_compare(R, C, VERIFY_EPS).ok;
            MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (!ok) {
                if (rank == 0) fprintf(stderr, "ERROR: %s con p = %d no coincide con el secuencial: no se mide\n", algs[a].name, p);
                MPI_Finalize();
                return 1;
            }
            double comm = 0, comp = 0, dist = 0;
            long long msgs = 0, bytes = 0;
            for (int rep = 0; rep < reps; rep++) {
                MPI_Barrier(MPI_COMM_WORLD);
                algs[a].f(A, B, C, MPI_COMM_WORLD, &st);
                mpi_stats_reduce(&st, &tot, MPI_COMM_WORLD);
                if (rank == 0) {
                    tt[rep] = tot.t_total;
                    comm = tot.t_comm; comp = tot.t_comp; dist = tot.t_dist; msgs = tot.msgs; bytes = tot.bytes;
                    printf("%s,%d,%d,%s,%d,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%.3f\n", algs[a].name, n, p, grid, rep + 1,
                           tot.t_total, tot.t_comm, tot.t_comp, tot.t_dist, tot.msgs, tot.bytes,
                           flops / tot.t_total * 1e-9);
                }
            }
            if (rank == 0) {
                fprintf(stderr, "  %-8s %5d %6s %10.4f %9.4f %9.4f %9.4f %9lld %10.1f\n", algs[a].name, n, grid,
                        median(tt, reps), comm, comp, dist, msgs, bytes / 1e6);
                fflush(stdout);
            }
        }
        mat_free(A); mat_free(B); mat_free(C); mat_free(R);
    }
    free(tt);
    MPI_Finalize();
    return 0;
}
