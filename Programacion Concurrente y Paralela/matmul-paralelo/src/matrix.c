#include "matrix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xcalloc(size_t n, size_t size) {
    void *p = calloc(n ? n : 1, size);
    if (!p) {
        fprintf(stderr, "Error: sin memoria (%zu x %zu bytes)\n", n, size);
        exit(EXIT_FAILURE);
    }
    return p;
}

Matrix *mat_alloc(int rows, int cols) {
    Matrix *M = xcalloc(1, sizeof *M);
    M->rows = rows;
    M->cols = cols;
    M->data = xcalloc((size_t)rows * (size_t)cols, sizeof(double));
    return M;
}

void mat_free(Matrix *M) {
    if (!M) return;
    free(M->data);
    free(M);
}

Matrix *mat_copy(const Matrix *A) {
    Matrix *B = mat_alloc(A->rows, A->cols);
    memcpy(B->data, A->data, (size_t)A->rows * (size_t)A->cols * sizeof(double));
    return B;
}

void mat_zero(Matrix *M) {
    memset(M->data, 0, (size_t)M->rows * (size_t)M->cols * sizeof(double));
}

Matrix *mat_transpose(const Matrix *A) {
    Matrix *T = mat_alloc(A->cols, A->rows);
    for (int i = 0; i < A->rows; i++)
        for (int j = 0; j < A->cols; j++)
            M_AT(T, j, i) = M_AT(A, i, j);
    return T;
}

void mat_print(const Matrix *M, const char *name) {
    int r = M->rows < 8 ? M->rows : 8;
    int c = M->cols < 8 ? M->cols : 8;
    printf("%s (%d x %d)%s\n", name, M->rows, M->cols,
           (r < M->rows || c < M->cols) ? " [recortada a 8x8]" : "");
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) printf("%10.4f ", M_AT(M, i, j));
        printf("\n");
    }
}

CSR *csr_alloc(int rows, int cols, int nnz) {
    CSR *A = xcalloc(1, sizeof *A);
    A->rows = rows;
    A->cols = cols;
    A->nnz = nnz;
    A->row_ptr = xcalloc((size_t)rows + 1, sizeof(int));
    A->col_idx = xcalloc((size_t)nnz, sizeof(int));
    A->val = xcalloc((size_t)nnz, sizeof(double));
    return A;
}

void csr_free(CSR *A) {
    if (!A) return;
    free(A->row_ptr);
    free(A->col_idx);
    free(A->val);
    free(A);
}

Matrix *csr_to_dense(const CSR *A) {
    Matrix *M = mat_alloc(A->rows, A->cols);
    for (int i = 0; i < A->rows; i++)
        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++)
            M_AT(M, i, A->col_idx[k]) += A->val[k];
    return M;
}

CSR *dense_to_csr(const Matrix *M) {
    int nnz = 0;
    for (size_t t = 0; t < (size_t)M->rows * (size_t)M->cols; t++)
        if (M->data[t] != 0.0) nnz++;

    CSR *A = csr_alloc(M->rows, M->cols, nnz);
    int k = 0;
    for (int i = 0; i < M->rows; i++) {
        A->row_ptr[i] = k;
        for (int j = 0; j < M->cols; j++) {
            double v = M_AT(M, i, j);
            if (v != 0.0) {
                A->col_idx[k] = j;
                A->val[k] = v;
                k++;
            }
        }
    }
    A->row_ptr[M->rows] = k;
    return A;
}
