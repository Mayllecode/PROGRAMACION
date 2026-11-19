/*
 * test_omp.c — Verificación de las versiones OpenMP (semana 7, Hito H2).
 *
 * Criterio del Hito H2: "OpenMP coincide con el secuencial con 1 a 8 hilos".
 * Cada grupo se repite con p = 1, 2, 3, 4 y 8 hilos (3 no divide a casi
 * ningún tamaño: prueba que el reparto disparejo también funciona).
 *
 * La comparación es EXACTA (bit a bit), no solo dentro de ε: en las
 * versiones OpenMP cada elemento se suma en el mismo orden que en la
 * secuencial, así que no hay ninguna razón para que cambie ni un bit. Si
 * alguna vez falla, hay un error de reparto o una condición de carrera.
 *
 * Grupos:
 *   1. Los hilos existen de verdad (se cuentan dentro de una región paralela)
 *   2. Ejemplo a mano con más hilos que filas
 *   3. Producto denso: ikj y por bloques (7, 32, 64) contra naive
 *   4. Producto disperso K·U contra spmm_csr
 *   5. Simulador: solo K·U en paralelo, y el simulador completo en paralelo
 *   6. Errores de dimensiones
 *
 * Uso: ./build/test_omp      (devuelve 0 si todo pasa, 1 si algo falla)
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "building.h"
#include "generators.h"
#include "ground_motion.h"
#include "matrix.h"
#include "par_omp.h"
#include "seq.h"
#include "sim.h"
#include "verify.h"

#define PI 3.14159265358979323846

static const int HILOS[] = {1, 2, 3, 4, 8};
#define N_HILOS ((int)(sizeof HILOS / sizeof HILOS[0]))

static int n_ok = 0, n_fail = 0;

static void check(int cond, const char *msg) {
    if (cond) { n_ok++;   printf("  [OK]    %s\n", msg); }
    else      { n_fail++; printf("  [FALLA] %s\n", msg); }
}

static void test_threads_exist(void) {
    printf("\n1) Se crean de verdad p hilos\n");
    for (int h = 0; h < N_HILOS; h++) {
        int p = HILOS[h], count = 0;
        omp_set_num_threads(p);
        #pragma omp parallel reduction(+ : count)
        count += 1;
        char msg[96];
        snprintf(msg, sizeof msg, "p = %d: la region paralela tiene %d hilos", p, count);
        check(count == p, msg);
    }
}

static void test_hand_example(void) {
    printf("\n2) Ejemplo a mano [2x3] x [3x2] con 8 hilos (mas hilos que filas)\n");
    Matrix *A = mat_alloc(2, 3), *B = mat_alloc(3, 2), *C = mat_alloc(2, 2);
    const double a[] = {1, 2, 3, 4, 5, 6}, b[] = {7, 8, 9, 10, 11, 12};
    for (int t = 0; t < 6; t++) { A->data[t] = a[t]; B->data[t] = b[t]; }
    omp_set_num_threads(8);
    matmul_omp_ikj(A, B, C);
    check(M_AT(C, 0, 0) == 58 && M_AT(C, 0, 1) == 64 && M_AT(C, 1, 0) == 139 && M_AT(C, 1, 1) == 154,
          "matmul_omp_ikj da [[58,64],[139,154]]");
    mat_zero(C);
    matmul_omp_blocked(A, B, C, 64);
    check(M_AT(C, 0, 0) == 58 && M_AT(C, 0, 1) == 64 && M_AT(C, 1, 0) == 139 && M_AT(C, 1, 1) == 154,
          "matmul_omp_blocked (bloque 64 > matriz) da [[58,64],[139,154]]");
    mat_free(A); mat_free(B); mat_free(C);
}

typedef int (*matmul_fn)(const Matrix *, const Matrix *, Matrix *);
static int omp_blocked7(const Matrix *A, const Matrix *B, Matrix *C)  { return matmul_omp_blocked(A, B, C, 7); }
static int omp_blocked32(const Matrix *A, const Matrix *B, Matrix *C) { return matmul_omp_blocked(A, B, C, 32); }
static int omp_blocked64(const Matrix *A, const Matrix *B, Matrix *C) { return matmul_omp_blocked(A, B, C, 64); }

static void test_dense(void) {
    printf("\n3) Producto denso contra naive (37x37, 50x23 * 23x61, 129x65 * 65x100)\n");
    const struct { const char *name; matmul_fn f; } algos[] = {
        {"omp_ikj", matmul_omp_ikj}, {"omp_blocked-7", omp_blocked7},
        {"omp_blocked-32", omp_blocked32}, {"omp_blocked-64", omp_blocked64},
    };
    const int dims[][3] = {{37, 37, 37}, {50, 23, 61}, {129, 65, 100}};  /* n x p  *  p x m */
    for (int h = 0; h < N_HILOS; h++) {
        omp_set_num_threads(HILOS[h]);
        for (int a = 0; a < 4; a++) {
            int same = 1;
            for (int d = 0; d < 3; d++) {
                Matrix *A = gen_random(dims[d][0], dims[d][1], 11 + d);
                Matrix *B = gen_random(dims[d][1], dims[d][2], 21 + d);
                Matrix *R = mat_alloc(dims[d][0], dims[d][2]), *C = mat_alloc(dims[d][0], dims[d][2]);
                for (size_t t = 0; t < (size_t)C->rows * C->cols; t++) C->data[t] = 99.0; /* basura: hay que sobrescribirla */
                matmul_naive(A, B, R);
                algos[a].f(A, B, C);
                same = same && mat_equal_exact(R, C);
                mat_free(A); mat_free(B); mat_free(R); mat_free(C);
            }
            char msg[96];
            snprintf(msg, sizeof msg, "p = %d  %-15s 3 tamanos, identico bit a bit", HILOS[h], algos[a].name);
            check(same, msg);
        }
    }
}

static void test_sparse(void) {
    printf("\n4) Producto disperso K*U contra spmm_csr\n");
    CSR *K1 = gen_stiffness_2d(37, 23);
    CSR *K2 = gen_random_sparse(300, 200, 0.05, 7);
    Building *b = building_create(10, 4, 2e4, 3.5e7, 1e8, 3.0);
    const CSR *Ks[] = {K1, K2, b->K};
    const int ks[] = {16, 5, 4};
    for (int h = 0; h < N_HILOS; h++) {
        omp_set_num_threads(HILOS[h]);
        int same = 1;
        for (int c = 0; c < 3; c++) {
            Matrix *U = gen_random(Ks[c]->cols, ks[c], 31 + c);
            Matrix *R = mat_alloc(Ks[c]->rows, ks[c]), *C = mat_alloc(Ks[c]->rows, ks[c]);
            for (size_t t = 0; t < (size_t)C->rows * C->cols; t++) C->data[t] = 99.0;
            spmm_csr(Ks[c], U, R);
            spmm_csr_omp(Ks[c], U, C);
            same = same && mat_equal_exact(R, C);
            mat_free(U); mat_free(R); mat_free(C);
        }
        char msg[110];
        snprintf(msg, sizeof msg, "p = %d  rigidez 2D 851 nodos, aleatoria 300x200, edificio 10x4: identico", HILOS[h]);
        check(same, msg);
    }
    /* La variante secuencial agregada en S7 (la T(1) del caso disperso) */
    int same = 1;
    for (int c = 0; c < 3; c++) {
        Matrix *U = gen_random(Ks[c]->cols, ks[c], 31 + c);
        Matrix *R = mat_alloc(Ks[c]->rows, ks[c]), *C = mat_alloc(Ks[c]->rows, ks[c]);
        for (size_t t = 0; t < (size_t)C->rows * C->cols; t++) C->data[t] = 99.0;
        spmm_csr(Ks[c], U, R);
        spmm_csr_rowinit(Ks[c], U, C);
        same = same && mat_equal_exact(R, C);
        mat_free(U); mat_free(R); mat_free(C);
    }
    check(same, "secuencial spmm_csr_rowinit (T(1) del disperso): identico a spmm_csr");
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

static void test_simulator(void) {
    printf("\n5) Simulador: edificio 12x5 ante 4 sismos, y vibracion libre con u0 y v0\n");
    const int k = 4;
    Building *b = building_create(12, 5, 2e4, 3.5e7, 1e8, 3.0);
    double T1 = building_fundamental_period(b);
    double dt = 0.5 * sim_dt_critical(b->K, b->mass);
    double alpha = 2.0 * 0.05 * (2.0 * PI / T1);
    int N = 800;
    Matrix *ag = mat_alloc(N, k);
    double *col = xcalloc((size_t)N, sizeof(double));
    const double freqs[] = {0.5, 1.0, 2.0, 4.0};
    for (int s = 0; s < k; s++) {
        ground_motion_synthetic(col, N, dt, 0.3 * G_ACCEL, freqs[s]);
        for (int t = 0; t < N; t++) M_AT(ag, t, s) = col[t];
    }
    const int roof = building_node(b, b->pisos, 0);
    SimConfig cfg_seq = {dt, N, k, alpha, spmm_csr, roof};
    SimConfig cfg_omp = {dt, N, k, alpha, spmm_csr_omp, roof};
    SimResult ref, r;
    sim_run(b->K, b->mass, b->below, &cfg_seq, ag, NULL, NULL, &ref);

    /* Vibración libre: sin sismo, sin deriva (below = NULL), con u0 y v0 */
    Matrix *u0 = gen_random(b->n, 2, 41), *v0 = gen_random(b->n, 2, 42);
    SimConfig free_seq = {dt, 300, 2, 0.0, spmm_csr, -1};
    SimConfig free_omp = {dt, 300, 2, 0.0, spmm_csr_omp, -1};
    SimResult ref_free, r_free;
    sim_run(b->K, b->mass, NULL, &free_seq, NULL, u0, v0, &ref_free);

    for (int h = 0; h < N_HILOS; h++) {
        omp_set_num_threads(HILOS[h]);
        char msg[110];
        sim_run(b->K, b->mass, b->below, &cfg_omp, ag, NULL, NULL, &r);
        snprintf(msg, sizeof msg, "p = %d  solo K*U en paralelo (sim_run + spmm_csr_omp): identico", HILOS[h]);
        check(same_result(&ref, &r, (N + 1) * k), msg);
        sim_result_free(&r);

        sim_run_omp(b->K, b->mass, b->below, &cfg_omp, ag, NULL, NULL, &r);
        snprintf(msg, sizeof msg, "p = %d  simulador completo (sim_run_omp): u, picos, derivas e historia", HILOS[h]);
        check(same_result(&ref, &r, (N + 1) * k), msg);
        sim_result_free(&r);

        sim_run_omp(b->K, b->mass, NULL, &free_omp, NULL, u0, v0, &r_free);
        snprintf(msg, sizeof msg, "p = %d  vibracion libre con u0 y v0 (sim_run_omp): identico", HILOS[h]);
        check(same_result(&ref_free, &r_free, 0), msg);
        sim_result_free(&r_free);
    }
    sim_result_free(&ref); sim_result_free(&ref_free);
    mat_free(ag); mat_free(u0); mat_free(v0); free(col); building_free(b);
}

static void test_errors(void) {
    printf("\n6) Dimensiones que no calzan se rechazan (devuelven -1)\n");
    omp_set_num_threads(4);
    Matrix *A = mat_alloc(4, 3), *B = mat_alloc(4, 2), *C = mat_alloc(4, 2);
    CSR *K = gen_stiffness_2d(2, 2);   /* 4 x 4 */
    Matrix *U = mat_alloc(3, 2);       /* debería tener 4 filas */
    check(matmul_omp_ikj(A, B, C) == -1 && matmul_omp_blocked(A, B, C, 8) == -1 &&
          matmul_omp_blocked(A, A, C, 0) == -1, "producto denso: A.cols != B.rows y bloque 0");
    check(spmm_csr_omp(K, U, C) == -1, "K*U: K de 4 columnas por U de 3 filas");
    SimConfig bad = {0.0, 10, 2, 0.0, spmm_csr_omp, -1};   /* dt = 0 */
    double mass[4] = {1, 1, 1, 1};
    SimResult r;
    check(sim_run_omp(K, mass, NULL, &bad, NULL, NULL, NULL, &r) == -1, "simulador: dt = 0");
    mat_free(A); mat_free(B); mat_free(C); mat_free(U); csr_free(K);
}

int main(void) {
    printf("=== Suite de verificacion — versiones OpenMP ===\n");
    printf("Nucleos logicos que ve OpenMP: %d. Cada grupo se repite con p = 1, 2, 3, 4 y 8 hilos.\n",
           omp_get_num_procs());
    printf("Criterio: resultado IDENTICO bit a bit al secuencial (mismo orden de las sumas).\n");
    omp_set_dynamic(0);   /* que OpenMP no reduzca el número de hilos por su cuenta */
    test_threads_exist();
    test_hand_example();
    test_dense();
    test_sparse();
    test_simulator();
    test_errors();
    printf("\n=== Resultado: %d pruebas OK, %d fallas ===\n", n_ok, n_fail);
    return n_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
