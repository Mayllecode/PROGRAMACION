/*
 * carrera_mpi.c — "La carrera": el MISMO edificio y los MISMOS sismos con
 * 1, 2, 4, 8... procesos, para ver en vivo cuánto más rápido va y que el
 * resultado no cambia. Es el último paso de DEMO_MPI.bat.
 *
 * Uso (una corrida por cada número de procesos, siempre empezando por 1):
 *     mpiexec -n 1 ./build/carrera_mpi 100 400 20
 *     mpiexec -n 8 ./build/carrera_mpi 100 400 20
 *                                     ^pisos ^vanos ^segundos de sismo
 *
 * Con 1 proceso corre el simulador SECUENCIAL (sim_run) y guarda su tiempo
 * y una huella (hash) del resultado en build/carrera_ref.txt. Con más
 * procesos corre sim_run_mpi y compara contra ese archivo:
 *   - tiempo y "N veces más rápido" que el secuencial,
 *   - "resultado idéntico: SI" si el desplazamiento final y las derivas
 *     tienen exactamente los mismos bits.
 * El tiempo es de reloj de pared en el proceso 0, de principio a fin
 * (incluye repartir el edificio y juntar los resultados).
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "building.h"
#include "ground_motion.h"
#include "par_mpi.h"
#include "seq.h"
#include "sim.h"

#define PI 3.14159265358979323846
#define K_SISMOS 8
#define REF "build/carrera_ref.txt"
#define BARRA 28

/* Huella FNV-1a de los bytes de una matriz: misma huella = mismos bits */
static unsigned long long huella(const Matrix *M, unsigned long long h) {
    const unsigned char *b = (const unsigned char *)M->data;
    size_t n = (size_t)M->rows * M->cols * sizeof(double);
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, p;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    const int pisos = argc > 1 ? atoi(argv[1]) : 100;
    const int vanos = argc > 2 ? atoi(argv[2]) : 400;
    const double segundos = argc > 3 ? atof(argv[3]) : 20.0;
    if (pisos < 1 || vanos < 0 || segundos <= 0.0) {
        if (rank == 0) fprintf(stderr, "Uso: mpiexec -n P %s pisos vanos segundos\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    if (rank != 0) {   /* los demás procesos solo calculan su franja */
        sim_run_mpi(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, MPI_COMM_WORLD, PART_NNZ, NULL);
        MPI_Finalize();
        return 0;
    }

    Building *b = building_create(pisos, vanos, 2.0e4, 3.5e7, 1.0e8, 3.0);
    const double T1 = building_fundamental_period(b);
    const double dt = 0.5 * sim_dt_critical(b->K, b->mass);
    const double alpha = 2.0 * 0.05 * (2.0 * PI / T1);
    const int N = (int)(segundos / dt);
    Matrix *ag = mat_alloc(N, K_SISMOS);
    double *col = xcalloc((size_t)N, sizeof(double));
    for (int s = 0; s < K_SISMOS; s++) {
        ground_motion_synthetic(col, N, dt, 0.3 * G_ACCEL, 0.5 + 3.5 * s / (K_SISMOS - 1));
        for (int t = 0; t < N; t++) M_AT(ag, t, s) = col[t];
    }
    SimConfig cfg = {dt, N, K_SISMOS, alpha, spmm_csr, -1};
    SimResult r;

    double t0 = MPI_Wtime();
    if (p == 1) sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r);
    else sim_run_mpi(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r, MPI_COMM_WORLD, PART_NNZ, NULL);
    const double t = MPI_Wtime() - t0;
    const unsigned long long h = huella(r.peak_drift, huella(r.u, 1469598103934665603ULL));

    if (p == 1) {
        printf("Edificio %d pisos x %d vanos = %d nodos | %d sismos a la vez | %.0f s de sismo (%d pasos)\n",
               pisos, vanos, b->n, K_SISMOS, segundos, N);
        printf("Cada paso multiplica K (%d x %d, %d no nulos) por U (%d x %d)\n\n", b->n, b->n, b->K->nnz, b->n, K_SISMOS);
        printf("  %-20s %8s   %s\n", "quien calcula", "tiempo", "(barra mas corta = mas rapido)");
        FILE *f = fopen(REF, "w");
        if (f) { fprintf(f, "%d %d %.3f %.6f %llu\n", pisos, vanos, segundos, t, h); fclose(f); }
        else fprintf(stderr, "Aviso: no se pudo escribir %s (existe la carpeta build?)\n", REF);
        printf("  %-20s %7.2f s   ", "1 proceso (secuenc.)", t);
        for (int i = 0; i < BARRA; i++) putchar('#');
        printf("\n");
    } else {
        int rp = 0, rv = 0;
        double rs = 0, tseq = 0;
        unsigned long long rh = 0;
        FILE *f = fopen(REF, "r");
        int ok = f && fscanf(f, "%d %d %lf %lf %llu", &rp, &rv, &rs, &tseq, &rh) == 5 &&
                 rp == pisos && rv == vanos && (int)(rs * 1000 + 0.5) == (int)(segundos * 1000 + 0.5);
        if (f) fclose(f);
        if (!ok) {
            printf("  %d procesos: %.2f s (primero hay que correr con -n 1 el mismo edificio)\n", p, t);
        } else {
            char nombre[32];
            snprintf(nombre, sizeof nombre, "%d procesos MPI", p);
            int n = (int)(BARRA * t / tseq + 0.5);
            if (n < 1) n = 1;
            if (n > BARRA) n = BARRA;
            printf("  %-20s %7.2f s   ", nombre, t);
            for (int i = 0; i < n; i++) putchar('#');
            printf("%*s  %.1f veces mas rapido | identico: %s\n", BARRA - n, "", tseq / t, h == rh ? "SI" : "NO (!)");
        }
    }
    fflush(stdout);
    sim_result_free(&r);
    mat_free(ag); free(col); building_free(b);
    MPI_Finalize();
    return 0;
}
