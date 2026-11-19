/*
 * bench_sim_mpi.c — Speedup del simulador con la estructura repartida (semana 11).
 *
 * Uso (una corrida por cada número de procesos; make bench-mpi las hace todas):
 *     mpiexec -n 1 ./build/bench_sim_mpi 5 400 8 >  results/mpi_sim_mipc.csv
 *     mpiexec -n 4 ./build/bench_sim_mpi 5 400 8 -noheader >> results/mpi_sim_mipc.csv
 *
 * Mismos edificios, pasos y sismos que bench_sim_omp (S7), para comparar.
 * Con p = 1 mide además T(1): el mejor simulador secuencial (sim_run con
 * spmm_csr o spmm_csr_rowinit).
 *
 * Versiones:
 *   sim_mpi_filas  reparto con el mismo número de nodos por proceso
 *   sim_mpi_nnz    reparto con el mismo número de no nulos por proceso
 *
 * Cada versión se VERIFICA antes de medir: desplazamiento final y derivas
 * idénticos bit a bit al secuencial.
 *
 * CSV (stdout):
 *   algoritmo,n,p,repeticion,tiempo_s,t_comm_s,t_comp_s,t_dist_s,mensajes,bytes,
 *   pisos,vanos,k,pasos,max_fantasmas,max_vecinos,desbalance
 */
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "building.h"
#include "ground_motion.h"
#include "par_mpi.h"
#include "seq.h"
#include "sim.h"
#include "verify.h"

#define PI 3.14159265358979323846

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
    int steps = argc > 2 ? atoi(argv[2]) : 400;
    int k = argc > 3 ? atoi(argv[3]) : 8;
    int header = !(argc > 4 && strcmp(argv[4], "-noheader") == 0);
    if (reps < 1 || steps < 1 || k < 1) {
        if (rank == 0) fprintf(stderr, "repeticiones, pasos y escenarios >= 1\n");
        MPI_Finalize();
        return 1;
    }
    const int sizes[][2] = {{20, 50}, {50, 200}, {100, 400}};
    const int n_sizes = 3;
    double *tt = malloc(reps * sizeof(double));
    if (rank == 0) {
        if (header)
            printf("algoritmo,n,p,repeticion,tiempo_s,t_comm_s,t_comp_s,t_dist_s,mensajes,bytes,"
                   "pisos,vanos,k,pasos,max_fantasmas,max_vecinos,desbalance\n");
        fprintf(stderr, "\nSimulador con MPI: p = %d procesos, %d pasos, %d escenarios, mediana de %d\n",
                p, steps, k, reps);
    }

    for (int z = 0; z < n_sizes; z++) {
        const int pisos = sizes[z][0], vanos = sizes[z][1];
        /* Todos arman el edificio (es barato); solo la raíz lo entrega a sim_run_mpi */
        Building *b = building_create(pisos, vanos, 2.0e4, 3.5e7, 1.0e8, 3.0);
        const double T1b = building_fundamental_period(b);
        const double dt = 0.5 * sim_dt_critical(b->K, b->mass);
        const double alpha = 2.0 * 0.05 * (2.0 * PI / T1b);
        Matrix *ag = mat_alloc(steps, k);
        double *col = malloc(steps * sizeof(double));
        for (int s = 0; s < k; s++) {
            ground_motion_synthetic(col, steps, dt, 0.3 * G_ACCEL, 0.5 + 3.5 * s / (k > 1 ? k - 1 : 1));
            for (int t = 0; t < steps; t++) M_AT(ag, t, s) = col[t];
        }
        SimConfig cfg = {dt, steps, k, alpha, spmm_csr, -1};
        SimResult ref, r;
        if (rank == 0) {
            sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &ref);
            fprintf(stderr, "\nEdificio %d x %d (n = %d nodos, nnz = %d)\n", pisos, vanos, b->n, b->K->nnz);
            fprintf(stderr, "  %-15s %3s %10s %9s %9s %9s %10s %8s %10s\n", "version", "p", "mediana_s", "comm_s",
                    "calc_s", "reparto_s", "mensajes", "MB", "fantasmas");
        }

        if (p == 1) {   /* T(1): el mejor secuencial */
            const struct { const char *name; spmm_fn f; } seqs[] = {
                {"sim_seq", spmm_csr}, {"sim_seq_rowinit", spmm_csr_rowinit},
            };
            for (int a = 0; a < 2; a++) {
                SimConfig c2 = cfg;
                c2.spmm = seqs[a].f;
                for (int rep = 0; rep < reps; rep++) {
                    sim_run(b->K, b->mass, b->below, &c2, ag, NULL, NULL, &r);
                    tt[rep] = r.t_total;
                    printf("%s,%d,1,%d,%.6f,0,%.6f,0,0,0,%d,%d,%d,%d,0,0,1.000\n", seqs[a].name, b->n, rep + 1,
                           r.t_total, r.t_total, pisos, vanos, k, steps);
                    sim_result_free(&r);
                }
                fprintf(stderr, "  %-15s %3d %10.4f\n", seqs[a].name, 1, median(tt, reps));
            }
        }

        const struct { const char *name; int mode; } vers[] = {{"sim_mpi_filas", PART_ROWS}, {"sim_mpi_nnz", PART_NNZ}};
        int *starts = xcalloc((size_t)p + 1, sizeof(int));
        for (int v = 0; v < 2; v++) {
            double imb = 1.0;
            if (rank == 0) { partition_rows(b->K, p, vers[v].mode, starts); imb = partition_imbalance(b->K, p, starts); }
            SimMpiStats st;
            MpiStats tot;
            /* verificación + calentamiento */
            sim_run_mpi(rank == 0 ? b->K : NULL, b->mass, b->below, &cfg, ag, NULL, NULL, rank == 0 ? &r : NULL,
                        MPI_COMM_WORLD, vers[v].mode, &st);
            int ok = 1;
            if (rank == 0) {
                ok = mat_equal_exact(ref.u, r.u) && mat_equal_exact(ref.peak_drift, r.peak_drift);
                sim_result_free(&r);
            }
            MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (!ok) {
                if (rank == 0) fprintf(stderr, "ERROR: %s con p = %d no da lo mismo que el secuencial\n", vers[v].name, p);
                MPI_Finalize();
                return 1;
            }
            int loc[2] = {st.ghosts, st.neighbors}, mx[2];
            MPI_Reduce(loc, mx, 2, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
            for (int rep = 0; rep < reps; rep++) {
                MPI_Barrier(MPI_COMM_WORLD);
                sim_run_mpi(rank == 0 ? b->K : NULL, b->mass, b->below, &cfg, ag, NULL, NULL,
                            rank == 0 ? &r : NULL, MPI_COMM_WORLD, vers[v].mode, &st);
                mpi_stats_reduce(&st.s, &tot, MPI_COMM_WORLD);
                if (rank == 0) {
                    tt[rep] = tot.t_total;
                    printf("%s,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%d,%d,%d,%d,%d,%d,%.3f\n", vers[v].name, b->n, p,
                           rep + 1, tot.t_total, tot.t_comm, tot.t_comp, tot.t_dist, tot.msgs, tot.bytes, pisos,
                           vanos, k, steps, mx[0], mx[1], imb);
                    sim_result_free(&r);
                }
            }
            if (rank == 0) {
                fprintf(stderr, "  %-15s %3d %10.4f %9.4f %9.4f %9.4f %10lld %8.1f %10d\n", vers[v].name, p,
                        median(tt, reps), tot.t_comm, tot.t_comp, tot.t_dist, tot.msgs, tot.bytes / 1e6, mx[0]);
                fflush(stdout);
            }
        }
        free(starts);
        if (rank == 0) sim_result_free(&ref);
        mat_free(ag); free(col); building_free(b);
    }
    free(tt);
    MPI_Finalize();
    return 0;
}
