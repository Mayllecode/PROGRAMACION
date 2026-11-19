/*
 * test_seq.c — Suite de verificación de la versión secuencial (Hito H1).
 *
 * Tres tipos de prueba:
 *   1. Casos con resultado conocido de antemano (a mano, identidad,
 *      permutación, matriz de rigidez por vector de unos).
 *   2. Consistencia: todas las variantes deben coincidir con matmul_naive,
 *      incluso con tamaños que NO son múltiplo del bloque.
 *   3. Formato disperso: CSR ida y vuelta, y SpMM contra la versión densa.
 *
 * Uso: ./build/test_seq      (devuelve 0 si todo pasa, 1 si algo falla)
 */
#include <stdio.h>
#include <stdlib.h>

#include "generators.h"
#include "matrix.h"
#include "seq.h"
#include "verify.h"

static int n_ok = 0, n_fail = 0;

static void check(int cond, const char *msg) {
    if (cond) { n_ok++;   printf("  [OK]    %s\n", msg); }
    else      { n_fail++; printf("  [FALLA] %s\n", msg); }
}

static void check_close(const Matrix *ref, const Matrix *x, const char *msg) {
    VerifyResult r = mat_compare(ref, x, VERIFY_EPS);
    verify_report(msg, r);
    if (r.ok) n_ok++; else n_fail++;
}

/* Firma común para probar las tres variantes densas con el mismo código */
typedef int (*matmul_fn)(const Matrix *, const Matrix *, Matrix *);
static int blocked8(const Matrix *A, const Matrix *B, Matrix *C)  { return matmul_blocked(A, B, C, 8); }
static int blocked7(const Matrix *A, const Matrix *B, Matrix *C)  { return matmul_blocked(A, B, C, 7); }
static int blocked32(const Matrix *A, const Matrix *B, Matrix *C) { return matmul_blocked(A, B, C, 32); }

static const struct { const char *name; matmul_fn f; } ALGOS[] = {
    {"naive",      matmul_naive},
    {"ikj",        matmul_ikj},
    {"blocked-7",  blocked7},
    {"blocked-8",  blocked8},
    {"blocked-32", blocked32},
};
#define N_ALGOS ((int)(sizeof ALGOS / sizeof ALGOS[0]))

static void test_hand_example(void) {
    printf("\n1) Ejemplo calculado a mano: [2x3] x [3x2]\n");
    Matrix *A = mat_alloc(2, 3), *B = mat_alloc(3, 2), *E = mat_alloc(2, 2);
    double a[] = {1, 2, 3, 4, 5, 6}, b[] = {7, 8, 9, 10, 11, 12}, e[] = {58, 64, 139, 154};
    for (int t = 0; t < 6; t++) { A->data[t] = a[t]; B->data[t] = b[t]; }
    for (int t = 0; t < 4; t++) E->data[t] = e[t];
    for (int k = 0; k < N_ALGOS; k++) {
        Matrix *C = mat_alloc(2, 2);
        char msg[96];
        snprintf(msg, sizeof msg, "%s da exactamente [[58,64],[139,154]]", ALGOS[k].name);
        check(ALGOS[k].f(A, B, C) == 0 && mat_equal_exact(E, C), msg);
        mat_free(C);
    }
    mat_free(A); mat_free(B); mat_free(E);
}

static void test_identity(void) {
    const int n = 37; /* impar y no múltiplo de ningún bloque: prueba los bordes */
    printf("\n2) Identidad (n = %d): A·I = A e I·A = A\n", n);
    Matrix *A = gen_random(n, n, 11), *I = gen_identity(n);
    for (int k = 0; k < N_ALGOS; k++) {
        Matrix *C = mat_alloc(n, n);
        char msg[96];
        ALGOS[k].f(A, I, C);
        snprintf(msg, sizeof msg, "%s: A·I == A (exacto)", ALGOS[k].name);
        check(mat_equal_exact(A, C), msg);
        ALGOS[k].f(I, A, C);
        snprintf(msg, sizeof msg, "%s: I·A == A (exacto)", ALGOS[k].name);
        check(mat_equal_exact(A, C), msg);
        mat_free(C);
    }
    mat_free(A); mat_free(I);
}

static void test_permutation(void) {
    const int n = 50;
    printf("\n3) Permutación (n = %d)\n", n);
    int *perm = malloc(n * sizeof(int));
    Matrix *P = gen_permutation(n, 2026, perm);
    Matrix *A = gen_random(n, n, 12);
    Matrix *PA = mat_alloc(n, n);
    matmul_blocked(P, A, PA, 16);
    int ok = 1;
    for (int i = 0; i < n && ok; i++)
        for (int j = 0; j < n; j++)
            if (M_AT(PA, i, j) != M_AT(A, perm[i], j)) { ok = 0; break; }
    check(ok, "fila i de P·A es exactamente la fila perm[i] de A");

    Matrix *Pt = mat_transpose(P), *PtP = mat_alloc(n, n), *I = gen_identity(n);
    matmul_ikj(Pt, P, PtP);
    check(mat_equal_exact(PtP, I), "P^T · P == I (P es ortogonal)");
    mat_free(Pt); mat_free(PtP); mat_free(I);
    mat_free(P); mat_free(A); mat_free(PA); free(perm);
}

static void test_consistency(void) {
    printf("\n4) Consistencia contra naive (rectangulares, bordes irregulares)\n");
    const int dims[][3] = {{1, 1, 1}, {129, 65, 100}, {64, 64, 64}, {200, 33, 71}};
    for (int d = 0; d < 4; d++) {
        int n = dims[d][0], p = dims[d][1], m = dims[d][2];
        Matrix *A = gen_random(n, p, 100 + d), *B = gen_random(p, m, 200 + d);
        Matrix *R = mat_alloc(n, m);
        matmul_naive(A, B, R);
        for (int k = 1; k < N_ALGOS; k++) {
            Matrix *C = mat_alloc(n, m);
            char msg[96];
            ALGOS[k].f(A, B, C);
            snprintf(msg, sizeof msg, "%s [%dx%d]·[%dx%d]", ALGOS[k].name, n, p, p, m);
            check_close(R, C, msg);
            mat_free(C);
        }
        mat_free(A); mat_free(B); mat_free(R);
    }
    Matrix *A = mat_alloc(3, 4), *B = mat_alloc(5, 2), *C = mat_alloc(3, 2);
    check(matmul_naive(A, B, C) == -1 && matmul_blocked(A, B, C, 4) == -1,
          "dimensiones incompatibles se rechazan (devuelve -1)");
    mat_free(A); mat_free(B); mat_free(C);
}

static void test_sparse(void) {
    printf("\n5) Formato disperso CSR\n");
    /* 5a. Tridiagonal a mano: [[2,-1,0],[-1,2,-1],[0,-1,2]] · [1,2,3]^T = [0,0,4]^T */
    Matrix *T = mat_alloc(3, 3);
    M_AT(T,0,0)=2;  M_AT(T,0,1)=-1;
    M_AT(T,1,0)=-1; M_AT(T,1,1)=2;  M_AT(T,1,2)=-1;
    M_AT(T,2,1)=-1; M_AT(T,2,2)=2;
    CSR *Ts = dense_to_csr(T);
    check(Ts->nnz == 7, "tridiagonal 3x3 tiene 7 no nulos");
    Matrix *x = mat_alloc(3, 1), *y = mat_alloc(3, 1);
    M_AT(x,0,0)=1; M_AT(x,1,0)=2; M_AT(x,2,0)=3;
    spmm_csr(Ts, x, y);
    check(M_AT(y,0,0)==0 && M_AT(y,1,0)==0 && M_AT(y,2,0)==4, "tridiagonal · [1,2,3] == [0,0,4] (exacto)");
    mat_free(T); csr_free(Ts); mat_free(x); mat_free(y);

    /* 5b. Matriz de rigidez 2D */
    const int nx = 12, ny = 9, n = nx * ny;
    CSR *K = gen_stiffness_2d(nx, ny);
    check(K->nnz == n + 2 * ((nx - 1) * ny + nx * (ny - 1)), "rigidez 2D: número de no nulos esperado");
    Matrix *Kd = csr_to_dense(K);
    Matrix *Kt = mat_transpose(Kd);
    check(mat_equal_exact(Kd, Kt), "rigidez 2D es simétrica (K == K^T)");
    CSR *K2 = dense_to_csr(Kd);
    Matrix *Kd2 = csr_to_dense(K2);
    check(K2->nnz == K->nnz && mat_equal_exact(Kd, Kd2), "CSR -> densa -> CSR conserva la matriz");

    /* K · 1 = 4 - (n.º de vecinos): 0 en nodos interiores, >0 en el borde */
    Matrix *ones = mat_alloc(n, 3), *r = mat_alloc(n, 3);
    for (size_t t = 0; t < (size_t)n * 3; t++) ones->data[t] = 1.0;
    spmm_csr(K, ones, r);
    int ok = 1;
    for (int yy = 0; yy < ny; yy++)
        for (int xx = 0; xx < nx; xx++) {
            int deg = (xx > 0) + (xx < nx - 1) + (yy > 0) + (yy < ny - 1);
            for (int c = 0; c < 3; c++)
                if (M_AT(r, yy * nx + xx, c) != 4.0 - deg) ok = 0;
        }
    check(ok, "K · 1 == 4 - vecinos en cada nodo (exacto)");

    /* 5c. SpMM contra la multiplicación densa */
    Matrix *B = gen_random(n, 16, 7), *Cs = mat_alloc(n, 16), *Cd = mat_alloc(n, 16);
    spmm_csr(K, B, Cs);
    matmul_naive(Kd, B, Cd);
    check_close(Cd, Cs, "spmm(K, B) == naive(K densa, B), rigidez");

    CSR *S = gen_random_sparse(150, 120, 0.05, 99);
    Matrix *Sd = csr_to_dense(S), *B2 = gen_random(120, 8, 8);
    Matrix *C1 = mat_alloc(150, 8), *C2 = mat_alloc(150, 8);
    spmm_csr(S, B2, C1);
    matmul_naive(Sd, B2, C2);
    check_close(C2, C1, "spmm == naive, dispersa aleatoria 5%");

    csr_free(K); csr_free(K2); mat_free(Kd); mat_free(Kt); mat_free(Kd2);
    mat_free(ones); mat_free(r); mat_free(B); mat_free(Cs); mat_free(Cd);
    csr_free(S); mat_free(Sd); mat_free(B2); mat_free(C1); mat_free(C2);
}

static void test_reproducible(void) {
    printf("\n6) Reproducibilidad de los generadores\n");
    Matrix *A = gen_random(40, 40, 42), *B = gen_random(40, 40, 42), *C = gen_random(40, 40, 43);
    check(mat_equal_exact(A, B), "misma semilla -> misma matriz");
    check(!mat_equal_exact(A, C), "semilla distinta -> matriz distinta");
    mat_free(A); mat_free(B); mat_free(C);
}

static void test_verifier_detects_errors(void) {
    printf("\n7) El verificador detecta errores (si no, las pruebas no valen nada)\n");
    Matrix *R = gen_random(20, 20, 5), *X = mat_copy(R);
    M_AT(X, 7, 3) += 1e-14;
    check(mat_compare(R, X, VERIFY_EPS).ok, "diferencia de redondeo (1e-14) se acepta");
    M_AT(X, 7, 3) += 1e-3;
    VerifyResult v = mat_compare(R, X, VERIFY_EPS);
    check(!v.ok && v.bad_i == 7 && v.bad_j == 3, "error real (1e-3) se detecta en (7,3)");
    M_AT(X, 7, 3) = M_AT(R, 7, 3);
    M_AT(X, 0, 0) = 0.0 / 0.0;
    check(!mat_compare(R, X, VERIFY_EPS).ok, "un NaN se detecta");
    mat_free(R); mat_free(X);
}

int main(void) {
    printf("=== Suite de verificación — versión secuencial ===\n");
    printf("Tolerancia: |x - ref| <= %.0e * (1 + |ref|)\n", VERIFY_EPS);
    test_hand_example();
    test_identity();
    test_permutation();
    test_consistency();
    test_sparse();
    test_reproducible();
    test_verifier_detects_errors();
    printf("\n=== Resultado: %d pruebas OK, %d fallas ===\n", n_ok, n_fail);
    return n_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
