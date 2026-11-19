#include "generators.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* uniforme en [0, 1) con 53 bits de precisión */
static double urand(uint64_t *state) {
    return (double)(splitmix64(state) >> 11) * (1.0 / 9007199254740992.0);
}

Matrix *gen_identity(int n) {
    Matrix *I = mat_alloc(n, n);
    for (int i = 0; i < n; i++) M_AT(I, i, i) = 1.0;
    return I;
}

Matrix *gen_permutation(int n, unsigned long long seed, int *perm_out) {
    uint64_t s = seed;
    int *perm = xcalloc((size_t)n, sizeof(int));
    for (int i = 0; i < n; i++) perm[i] = i;
    for (int i = n - 1; i > 0; i--) {          /* Fisher–Yates */
        int j = (int)(splitmix64(&s) % (uint64_t)(i + 1));
        int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    Matrix *P = mat_alloc(n, n);
    for (int i = 0; i < n; i++) M_AT(P, i, perm[i]) = 1.0;
    if (perm_out) memcpy(perm_out, perm, (size_t)n * sizeof(int));
    free(perm);
    return P;
}

Matrix *gen_random(int rows, int cols, unsigned long long seed) {
    uint64_t s = seed;
    Matrix *M = mat_alloc(rows, cols);
    for (size_t t = 0; t < (size_t)rows * (size_t)cols; t++)
        M->data[t] = 2.0 * urand(&s) - 1.0;
    return M;
}

CSR *gen_stiffness_2d(int nx, int ny) {
    int n = nx * ny;
    int nnz = n + 2 * ((nx - 1) * ny + nx * (ny - 1));
    CSR *K = csr_alloc(n, n, nnz);
    int k = 0;
    for (int y = 0; y < ny; y++) {
        for (int x = 0; x < nx; x++) {
            int i = y * nx + x;
            K->row_ptr[i] = k;
            /* columnas en orden creciente: abajo, izq, diag, der, arriba */
            if (y > 0)      { K->col_idx[k] = i - nx; K->val[k++] = -1.0; }
            if (x > 0)      { K->col_idx[k] = i - 1;  K->val[k++] = -1.0; }
                              K->col_idx[k] = i;      K->val[k++] =  4.0;
            if (x < nx - 1) { K->col_idx[k] = i + 1;  K->val[k++] = -1.0; }
            if (y < ny - 1) { K->col_idx[k] = i + nx; K->val[k++] = -1.0; }
        }
    }
    K->row_ptr[n] = k;
    return K;
}

CSR *gen_random_sparse(int rows, int cols, double density, unsigned long long seed) {
    /* Dos pasadas con la misma semilla: primero contar, luego llenar. */
    uint64_t s = seed;
    int nnz = 0;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) {
            if (urand(&s) < density) { nnz++; (void)urand(&s); (void)urand(&s); }
        }

    CSR *A = csr_alloc(rows, cols, nnz);
    s = seed;
    int k = 0;
    for (int i = 0; i < rows; i++) {
        A->row_ptr[i] = k;
        for (int j = 0; j < cols; j++) {
            if (urand(&s) < density) {
                double mag = 1.0 + urand(&s);
                double sign = urand(&s) < 0.5 ? -1.0 : 1.0;
                A->col_idx[k] = j;
                A->val[k] = sign * mag;
                k++;
            }
        }
    }
    A->row_ptr[rows] = k;
    return A;
}
