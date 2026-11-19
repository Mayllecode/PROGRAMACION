#include "seq.h"

static int dims_ok(int ar, int ac, const Matrix *B, const Matrix *C) {
    return ac == B->rows && C->rows == ar && C->cols == B->cols;
}

int matmul_naive(const Matrix *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int n = A->rows, m = B->cols, p = A->cols;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++) {
            double s = 0.0;
            for (int k = 0; k < p; k++) s += M_AT(A, i, k) * M_AT(B, k, j);
            M_AT(C, i, j) = s;
        }
    return 0;
}

int matmul_ikj(const Matrix *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int n = A->rows, m = B->cols, p = A->cols;
    mat_zero(C);
    for (int i = 0; i < n; i++) {
        double *c = &M_AT(C, i, 0);
        for (int k = 0; k < p; k++) {
            const double a = M_AT(A, i, k);
            const double *b = &M_AT(B, k, 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
    return 0;
}

static int imin(int a, int b) { return a < b ? a : b; }

int matmul_blocked(const Matrix *A, const Matrix *B, Matrix *C, int bs) {
    if (bs <= 0 || !dims_ok(A->rows, A->cols, B, C)) return -1;
    const int n = A->rows, m = B->cols, p = A->cols;
    mat_zero(C);
    for (int ii = 0; ii < n; ii += bs)
        for (int kk = 0; kk < p; kk += bs)
            for (int jj = 0; jj < m; jj += bs) {
                const int iend = imin(ii + bs, n);
                const int kend = imin(kk + bs, p);
                const int jend = imin(jj + bs, m);
                for (int i = ii; i < iend; i++) {
                    double *c = &M_AT(C, i, 0);
                    for (int k = kk; k < kend; k++) {
                        const double a = M_AT(A, i, k);
                        const double *b = &M_AT(B, k, 0);
                        for (int j = jj; j < jend; j++) c[j] += a * b[j];
                    }
                }
            }
    return 0;
}

int spmm_csr(const CSR *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int m = B->cols;
    mat_zero(C);
    for (int i = 0; i < A->rows; i++) {
        double *c = &M_AT(C, i, 0);
        for (int t = A->row_ptr[i]; t < A->row_ptr[i + 1]; t++) {
            const double a = A->val[t];
            const double *b = &M_AT(B, A->col_idx[t], 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
    return 0;
}

int spmm_csr_rowinit(const CSR *A, const Matrix *B, Matrix *C) {
    if (!dims_ok(A->rows, A->cols, B, C)) return -1;
    const int m = B->cols;
    for (int i = 0; i < A->rows; i++) {
        double *c = &M_AT(C, i, 0);
        for (int j = 0; j < m; j++) c[j] = 0.0;   /* la fila se limpia aquí, ya en caché */
        for (int t = A->row_ptr[i]; t < A->row_ptr[i + 1]; t++) {
            const double a = A->val[t];
            const double *b = &M_AT(B, A->col_idx[t], 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
    return 0;
}
