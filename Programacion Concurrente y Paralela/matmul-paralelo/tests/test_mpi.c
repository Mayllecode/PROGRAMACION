/*
 * test_mpi.c — Verificación de las versiones MPI (semanas 9 a 11, Hito H3).
 *
 * Criterio del Hito H3: "Cannon y SUMMA verificados; la simulación con MPI
 * da los mismos desplazamientos que la secuencial con 1, 4, 9 y 16 procesos".
 *
 * Se corre con mpiexec, una vez por cada número de procesos:
 *     mpiexec -n 4 ./build/test_mpi
 * (make test-mpi lo hace con 1, 2, 4, 6, 9 y 16). La referencia secuencial
 * se calcula en el proceso 0 dentro del mismo programa.
 *
 * Criterios de comparación:
 *   - Cannon y SUMMA: dentro de ε (verify.h). Cada proceso suma su parte del
 *     índice interno y después se suman los bloques: el orden de las sumas
 *     cambia respecto al secuencial y pueden cambiar los últimos bits.
 *   - K·U y simulador: IDÉNTICO bit a bit. Cada fila de K·U la calcula un
 *     solo proceso, con sus no nulos en el mismo orden que spmm_csr.
 *
 * Grupos:
 *   1. Los procesos existen y la malla es la esperada
 *   2. Ejemplo a mano (más procesos que filas: casi todo es relleno)
 *   3. Cannon contra naive (cuadradas, impares, rectangulares)
 *   4. SUMMA contra naive (los mismos casos; cualquier p)
 *   5. Reparto de la estructura: por filas y por no nulos
 *   6. K·U distribuido con intercambio de fronteras
 *   7. Simulador distribuido: desplazamientos, picos, derivas e historia
 *   8. Errores: dimensiones que no calzan, p no cuadrado en Cannon
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "building.h"
#include "generators.h"
#include "ground_motion.h"
#include "matrix.h"
#include "par_mpi.h"
#include "seq.h"
#include "sim.h"
#include "verify.h"

#define PI 3.14159265358979323846

static int rank = 0, nprocs = 1, n_ok = 0, n_fail = 0;

/* Solo la raíz decide e imprime; el resultado se comparte para que todos
 * los procesos terminen con el mismo código de salida. */
static void check(int cond, const char *msg) {
    if (rank != 0) return;
    if (cond) { n_ok++;   printf("  [OK]    %s\n", msg); }
    else      { n_fail++; printf("  [FALLA] %s\n", msg); }
    fflush(stdout);
}

static void title(const char *t) {
    if (rank == 0) { printf("\n%s\n", t); fflush(stdout); }
}

static void test_processes(void) {
    title("1) Los procesos existen y la malla es la esperada");
    int sum = 0;
    MPI_Allreduce(&rank, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    char msg[120];
    snprintf(msg, sizeof msg, "p = %d procesos: la suma de los rangos es %d (esperado %d)", nprocs, sum,
             nprocs * (nprocs - 1) / 2);
    check(sum == nprocs * (nprocs - 1) / 2, msg);
    int q = mpi_square_side(nprocs), pr, pc;
    mpi_grid_dims(nprocs, &pr, &pc);
    if (q) snprintf(msg, sizeof msg, "Cannon: malla %d x %d", q, q);
    else   snprintf(msg, sizeof msg, "Cannon: p = %d no es cuadrado perfecto (se probara que lo rechaza)", nprocs);
    check(q == 0 || q * q == nprocs, msg);
    snprintf(msg, sizeof msg, "SUMMA: malla %d x %d (pr * pc = p, lo mas cuadrada posible)", pr, pc);
    check(pr * pc == nprocs && pr <= pc, msg);
}

typedef int (*mpi_mm)(const Matrix *, const Matrix *, Matrix *, MPI_Comm, MpiStats *);

static void test_hand_example(void) {
    title("2) Ejemplo a mano [2x3] x [3x2] (con p grande, casi todo el bloque es relleno de ceros)");
    const double a[] = {1, 2, 3, 4, 5, 6}, b[] = {7, 8, 9, 10, 11, 12};
    const struct { const char *name; mpi_mm f; int needs_square; } algs[] = {
        {"Cannon", matmul_cannon, 1}, {"SUMMA", matmul_summa, 0},
    };
    for (int t = 0; t < 2; t++) {
        if (algs[t].needs_square && !mpi_square_side(nprocs)) continue;
        Matrix *A = mat_alloc(2, 3), *B = mat_alloc(3, 2), *C = mat_alloc(2, 2);
        for (int i = 0; i < 6; i++) { A->data[i] = a[i]; B->data[i] = b[i]; }
        algs[t].f(rank == 0 ? A : NULL, rank == 0 ? B : NULL, rank == 0 ? C : NULL, MPI_COMM_WORLD, NULL);
        char msg[80];
        snprintf(msg, sizeof msg, "%s da [[58,64],[139,154]]", algs[t].name);
        check(M_AT(C, 0, 0) == 58 && M_AT(C, 0, 1) == 64 && M_AT(C, 1, 0) == 139 && M_AT(C, 1, 1) == 154, msg);
        mat_free(A); mat_free(B); mat_free(C);
    }
}

static void test_dense(const char *name, mpi_mm f) {
    const int cases[][3] = {{64, 64, 64}, {37, 37, 37}, {50, 30, 70}, {1, 1, 1}, {100, 3, 5}};
    const int nc = (int)(sizeof cases / sizeof cases[0]);
    for (int c = 0; c < nc; c++) {
        const int n = cases[c][0], l = cases[c][1], m = cases[c][2];
        Matrix *A = gen_random(n, l, 100 + c), *B = gen_random(l, m, 200 + c);
        Matrix *R = mat_alloc(n, m), *C = mat_alloc(n, m);
        MpiStats st;
        int rc = f(rank == 0 ? A : NULL, rank == 0 ? B : NULL, rank == 0 ? C : NULL, MPI_COMM_WORLD, &st);
        char msg[140];
        if (rank == 0) {
            matmul_naive(A, B, R);
            VerifyResult v = mat_compare(R, C, VERIFY_EPS);
            snprintf(msg, sizeof msg, "%s [%dx%d]x[%dx%d]: error max %.1e  (%lld mensajes recibidos por el proceso 0)",
                     name, n, l, l, m, v.max_abs, st.msgs);
            check(rc == 0 && v.ok, msg);
        }
        mat_free(A); mat_free(B); mat_free(R); mat_free(C);
    }
}

static void test_cannon(void) {
    title("3) Cannon contra naive (dentro de eps = 1e-9)");
    if (!mpi_square_side(nprocs)) {
        if (rank == 0) printf("  (p = %d no es cuadrado: Cannon no aplica; ver grupo 8)\n", nprocs);
        return;
    }
    test_dense("Cannon", matmul_cannon);
}

static void test_summa(void) {
    title("4) SUMMA contra naive (dentro de eps = 1e-9), con cualquier p");
    test_dense("SUMMA ", matmul_summa);
}

/* Matriz con filas de largo muy distinto: el primer cuarto de las filas
 * tiene 40 no nulos y el resto 2. Repartir por filas queda desbalanceado. */
static CSR *gen_uneven(int n) {
    int nnz = 0;
    for (int i = 0; i < n; i++) nnz += (i < n / 4) ? 40 : 2;
    CSR *K = csr_alloc(n, n, nnz);
    int t = 0;
    for (int i = 0; i < n; i++) {
        K->row_ptr[i] = t;
        int len = (i < n / 4) ? 40 : 2;
        for (int x = 0; x < len; x++) {
            K->col_idx[t] = (int)(((long long)i + (long long)x * 7) % n);
            K->val[t++] = 1.0 + x;
        }
        /* columnas en orden creciente, como en todo CSR del proyecto */
        for (int u = K->row_ptr[i] + 1; u < t; u++)
            for (int v = u; v > K->row_ptr[i] && K->col_idx[v - 1] > K->col_idx[v]; v--) {
                int ci = K->col_idx[v]; K->col_idx[v] = K->col_idx[v - 1]; K->col_idx[v - 1] = ci;
                double cv = K->val[v]; K->val[v] = K->val[v - 1]; K->val[v - 1] = cv;
            }
    }
    K->row_ptr[n] = t;
    return K;
}

static void test_partition(void) {
    title("5) Reparto de la estructura entre procesos");
    if (rank != 0) return;   /* función local: no comunica */
    Building *b = building_create(30, 8, 2e4, 3.5e7, 1e8, 3.0);
    CSR *U = gen_uneven(400);
    const CSR *Ks[] = {b->K, U};
    const char *names[] = {"edificio 30x8", "filas desparejas (40 y 2 nnz)"};
    int *s = xcalloc((size_t)nprocs + 1, sizeof(int));
    for (int c = 0; c < 2; c++)
        for (int mode = 0; mode < 2; mode++) {
            partition_rows(Ks[c], nprocs, mode, s);
            int ok = s[0] == 0 && s[nprocs] == Ks[c]->rows;
            for (int r = 0; r < nprocs; r++) ok = ok && s[r] <= s[r + 1];
            char msg[140];
            snprintf(msg, sizeof msg, "%-30s por %-5s: rangos contiguos que cubren todo, desbalance %.2f",
                     names[c], mode == PART_NNZ ? "nnz" : "filas", partition_imbalance(Ks[c], nprocs, s));
            check(ok, msg);
        }
    if (nprocs > 1) {
        partition_rows(U, nprocs, PART_ROWS, s);
        double ir = partition_imbalance(U, nprocs, s);
        partition_rows(U, nprocs, PART_NNZ, s);
        double in = partition_imbalance(U, nprocs, s);
        char msg[140];
        snprintf(msg, sizeof msg, "filas desparejas: repartir por nnz baja el desbalance de %.2f a %.2f", ir, in);
        check(in < ir && in < 1.25, msg);
    }
    free(s); csr_free(U); building_free(b);
}

static void test_spmm(void) {
    title("6) K*U distribuido (reparto + intercambio de fronteras): identico a spmm_csr");
    Building *b = building_create(10, 4, 2e4, 3.5e7, 1e8, 3.0);
    CSR *K1 = gen_stiffness_2d(37, 23), *K2 = gen_random_sparse(300, 300, 0.02, 77);
    const CSR *Ks[] = {K1, K2, b->K};
    const int ks[] = {7, 3, 4};
    const char *names[] = {"rigidez 2D 851 nodos", "aleatoria 300x300 (vecinos en cualquier lado)", "edificio 10x4"};
    for (int c = 0; c < 3; c++)
        for (int mode = 0; mode < 2; mode++) {
            Matrix *U = gen_random(Ks[c]->cols, ks[c], 31 + c);
            Matrix *R = mat_alloc(Ks[c]->rows, ks[c]), *C = mat_alloc(Ks[c]->rows, ks[c]);
            MpiStats st;
            int rc = spmm_csr_mpi(rank == 0 ? Ks[c] : NULL, rank == 0 ? U : NULL, rank == 0 ? C : NULL,
                                  MPI_COMM_WORLD, mode, &st);
            MpiStats tot;
            mpi_stats_reduce(&st, &tot, MPI_COMM_WORLD);
            if (rank == 0) {
                spmm_csr(Ks[c], U, R);
                char msg[160];
                snprintf(msg, sizeof msg, "%-46s por %-5s: identico (%lld mensajes de frontera)",
                         names[c], mode == PART_NNZ ? "nnz" : "filas", tot.msgs);
                check(rc == 0 && mat_equal_exact(R, C), msg);
            }
            mat_free(U); mat_free(R); mat_free(C);
        }
    csr_free(K1); csr_free(K2); building_free(b);
}

static int same_result(const SimResult *a, const SimResult *b, int n_hist) {
    if (!mat_equal_exact(a->u, b->u) || !mat_equal_exact(a->peak_u, b->peak_u) ||
        !mat_equal_exact(a->peak_drift, b->peak_drift)) return 0;
    if ((a->history == NULL) != (b->history == NULL)) return 0;
    if (a->history)
        for (int t = 0; t < n_hist; t++) if (a->history[t] != b->history[t]) return 0;
    return 1;
}

static void sim_case(int pisos, int vanos, int k, int N, int with_quake, const char *label) {
    Building *b = building_create(pisos, vanos, 2e4, 3.5e7, 1e8, 3.0);
    double T1 = building_fundamental_period(b);
    double dt = 0.5 * sim_dt_critical(b->K, b->mass);
    double alpha = with_quake ? 2.0 * 0.05 * (2.0 * PI / T1) : 0.0;
    Matrix *ag = NULL, *u0 = NULL, *v0 = NULL;
    if (with_quake) {
        ag = mat_alloc(N, k);
        double *col = xcalloc((size_t)N, sizeof(double));
        for (int s = 0; s < k; s++) {
            ground_motion_synthetic(col, N, dt, 0.3 * G_ACCEL, 0.5 * (1 << s));
            for (int t = 0; t < N; t++) M_AT(ag, t, s) = col[t];
        }
        free(col);
    } else {
        u0 = gen_random(b->n, k, 41);
        v0 = gen_random(b->n, k, 42);
    }
    const int roof = with_quake ? building_node(b, b->pisos, 0) : -1;
    const int *below = with_quake ? b->below : NULL;
    SimConfig cfg = {dt, N, k, alpha, spmm_csr, roof};
    SimResult ref, r;
    if (rank == 0) sim_run(b->K, b->mass, below, &cfg, ag, u0, v0, &ref);
    for (int mode = 0; mode < 2; mode++) {
        SimMpiStats st;
        int rc = sim_run_mpi(rank == 0 ? b->K : NULL, rank == 0 ? b->mass : NULL, rank == 0 ? below : NULL,
                             rank == 0 ? &cfg : NULL, rank == 0 ? ag : NULL, rank == 0 ? u0 : NULL,
                             rank == 0 ? v0 : NULL, rank == 0 ? &r : NULL, MPI_COMM_WORLD, mode, &st);
        int ghosts = st.ghosts, maxg = 0;
        MPI_Reduce(&ghosts, &maxg, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            char msg[170];
            snprintf(msg, sizeof msg, "%s, reparto por %-5s: identico (max %d fantasmas por proceso)",
                     label, mode == PART_NNZ ? "nnz" : "filas", maxg);
            check(rc == 0 && same_result(&ref, &r, (N + 1) * k), msg);
            sim_result_free(&r);
        }
    }
    if (rank == 0) sim_result_free(&ref);
    mat_free(ag); mat_free(u0); mat_free(v0); building_free(b);
}

static void test_simulator(void) {
    title("7) Simulador distribuido: u final, picos, derivas e historia del techo identicos a sim_run");
    sim_case(12, 5, 4, 800, 1, "edificio 12x5, 4 sismos, 800 pasos  ");
    sim_case(12, 5, 2, 300, 0, "vibracion libre con u0 y v0         ");
    sim_case(3, 2, 3, 200, 1, "edificio 3x2 (9 nodos; p > pisos)   ");
}

static void test_errors(void) {
    title("8) Datos invalidos se rechazan (devuelven -1 en todos los procesos)");
    Matrix *A = mat_alloc(4, 3), *B = mat_alloc(4, 2), *C = mat_alloc(4, 2);
    int rc1 = matmul_summa(rank == 0 ? A : NULL, rank == 0 ? B : NULL, rank == 0 ? C : NULL, MPI_COMM_WORLD, NULL);
    int rc2 = matmul_cannon(rank == 0 ? A : NULL, rank == 0 ? B : NULL, rank == 0 ? C : NULL, MPI_COMM_WORLD, NULL);
    check(rc1 == -1 && rc2 == -1, "producto denso: A.cols != B.rows");
    if (!mpi_square_side(nprocs)) {
        Matrix *X = mat_alloc(4, 4), *Y = mat_alloc(4, 4);
        int rc = matmul_cannon(rank == 0 ? X : NULL, rank == 0 ? X : NULL, rank == 0 ? Y : NULL, MPI_COMM_WORLD, NULL);
        char msg[90];
        snprintf(msg, sizeof msg, "Cannon con p = %d (no cuadrado) devuelve -1", nprocs);
        check(rc == -1, msg);
        mat_free(X); mat_free(Y);
    }
    CSR *K = gen_stiffness_2d(2, 2);
    double mass[4] = {1, 1, 1, 1};
    SimConfig bad = {0.0, 10, 2, 0.0, spmm_csr, -1};
    SimResult r;
    int rc3 = sim_run_mpi(rank == 0 ? K : NULL, rank == 0 ? mass : NULL, NULL, rank == 0 ? &bad : NULL,
                          NULL, NULL, NULL, rank == 0 ? &r : NULL, MPI_COMM_WORLD, PART_ROWS, NULL);
    check(rc3 == -1, "simulador: dt = 0");
    mat_free(A); mat_free(B); mat_free(C); csr_free(K);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    if (rank == 0) {
        printf("=== Suite de verificacion — versiones MPI, p = %d procesos ===\n", nprocs);
        printf("Cannon y SUMMA: dentro de eps. K*U y simulador: IDENTICOS bit a bit.\n");
    }
    test_processes();
    test_hand_example();
    test_cannon();
    test_summa();
    test_partition();
    test_spmm();
    test_simulator();
    test_errors();
    int fails = n_fail;
    MPI_Bcast(&fails, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0) printf("\n=== Resultado (p = %d): %d pruebas OK, %d fallas ===\n", nprocs, n_ok, n_fail);
    MPI_Finalize();
    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
