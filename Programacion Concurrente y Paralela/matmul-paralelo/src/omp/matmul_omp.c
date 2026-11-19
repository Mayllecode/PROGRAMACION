/*
 * matmul_omp.c — Producto de matrices con OpenMP (semana 7).
 *
 * Cada función es la versión de seq.c con el trabajo repartido entre hilos.
 * Regla de todas: un hilo solo ESCRIBE en sus filas (o bloques) de C, así
 * que no hay condiciones de carrera ni hace falta sincronizar nada más que
 * la barrera implícita al final del "parallel for".
 *
 * schedule(static): las iteraciones se dividen en p trozos iguales y
 * contiguos, uno por hilo. Es lo adecuado aquí porque todas las filas
 * (o bloques) cuestan lo mismo; en K·U cada fila tiene como máximo 5 no
 * nulos, así que también están parejas.
 */
#include "par_omp.h"

static int dims_ok(int ar, int ac, const Matrix *B, const Matrix *C) {
    return ac == B->rows && C->rows == ar && C->cols == B->cols;
}

static int imin(int a, int b) { return a < b ? a : b; }

int matmul_omp_ikj(const Matrix *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int n = A->rows, m = B->cols, p = A->cols;

    /* Cada hilo recibe un trozo de filas i; dentro, el mismo i-k-j de seq.c */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        double *c = &M_AT(C, i, 0);
        for (int j = 0; j < m; j++) c[j] = 0.0;     /* cada hilo limpia SU fila */
        for (int k = 0; k < p; k++) {
            const double a = M_AT(A, i, k);
            const double *b = &M_AT(B, k, 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
    return 0;
}

int matmul_omp_blocked(const Matrix *A, const Matrix *B, Matrix *C, int bs) {
    if (bs <= 0 || !dims_ok(A->rows, A->cols, B, C)) return -1;
    const int n = A->rows, m = B->cols, p = A->cols;
    const int nbi = (n + bs - 1) / bs;   /* bloques de filas de C    */
    const int nbj = (m + bs - 1) / bs;   /* bloques de columnas de C */

    /* collapse(2): la "lista de tareas" es la malla nbi x nbj de bloques de C
     * (para n = 1024 y bs = 64: 256 bloques), no solo las nbi filas de
     * bloques; así sobra trabajo para repartir aunque haya muchos hilos.
     * El bucle kk queda DENTRO: un mismo bloque de C nunca lo tocan dos hilos. */
    #pragma omp parallel for collapse(2) schedule(static)
    for (int bi = 0; bi < nbi; bi++)
        for (int bj = 0; bj < nbj; bj++) {
            const int ii = bi * bs, iend = imin(ii + bs, n);
            const int jj = bj * bs, jend = imin(jj + bs, m);
            for (int i = ii; i < iend; i++)
                for (int j = jj; j < jend; j++) M_AT(C, i, j) = 0.0;
            for (int kk = 0; kk < p; kk += bs) {
                const int kend = imin(kk + bs, p);
                for (int i = ii; i < iend; i++) {
                    double *c = &M_AT(C, i, 0);
                    for (int k = kk; k < kend; k++) {
                        const double a = M_AT(A, i, k);
                        const double *b = &M_AT(B, k, 0);
                        for (int j = jj; j < jend; j++) c[j] += a * b[j];
                    }
                }
            }
        }
    return 0;
}

int spmm_csr_omp(const CSR *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int m = B->cols;

    /* Fila i de K = nodo i de la estructura. Cada hilo calcula las fuerzas
     * de SUS nodos para todos los escenarios (las m columnas de U). Leer
     * filas de U de nodos vecinos que "pertenecen" a otro hilo no es un
     * problema: durante el producto U solo se lee. */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < A->rows; i++) {
        double *c = &M_AT(C, i, 0);
        for (int j = 0; j < m; j++) c[j] = 0.0;
        for (int t = A->row_ptr[i]; t < A->row_ptr[i + 1]; t++) {
            const double a = A->val[t];
            const double *b = &M_AT(B, A->col_idx[t], 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
    return 0;
}
