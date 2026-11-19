#include "building.h"

#include <math.h>
#include <stdlib.h>

#include "seq.h"

#define PI 3.14159265358979323846

Building *building_create(int pisos, int vanos, double m, double kc, double kb, double h) {
    Building *b = xcalloc(1, sizeof *b);
    const int W = vanos + 1;
    b->pisos = pisos;
    b->vanos = vanos;
    b->n = pisos * W;
    b->h = h;
    b->mass = xcalloc((size_t)b->n, sizeof(double));
    b->below = xcalloc((size_t)b->n, sizeof(int));

    /* Contar no nulos: diagonal + vecinos verticales + vecinos horizontales */
    int nnz = b->n + 2 * (pisos - 1) * W + 2 * pisos * vanos;
    CSR *K = csr_alloc(b->n, b->n, nnz);
    int k = 0;
    for (int p = 1; p <= pisos; p++) {
        for (int c = 0; c <= vanos; c++) {
            int i = (p - 1) * W + c;
            b->mass[i] = m;
            b->below[i] = (p > 1) ? i - W : -1;

            /* Diagonal: suma de las rigideces de todos los resortes del nodo.
             * La columna de abajo siempre existe (si p = 1, va al suelo). */
            double diag = kc;
            if (p < pisos) diag += kc;
            if (c > 0) diag += kb;
            if (c < vanos) diag += kb;

            K->row_ptr[i] = k;  /* columnas en orden creciente */
            if (p > 1)     { K->col_idx[k] = i - W; K->val[k++] = -kc; }
            if (c > 0)     { K->col_idx[k] = i - 1; K->val[k++] = -kb; }
                             K->col_idx[k] = i;     K->val[k++] = diag;
            if (c < vanos) { K->col_idx[k] = i + 1; K->val[k++] = -kb; }
            if (p < pisos) { K->col_idx[k] = i + W; K->val[k++] = -kc; }
        }
    }
    K->row_ptr[b->n] = k;
    b->K = K;
    return b;
}

void building_free(Building *b) {
    if (!b) return;
    csr_free(b->K);
    free(b->mass);
    free(b->below);
    free(b);
}

double building_fundamental_period(const Building *b) {
    Matrix *phi = mat_alloc(b->n, 1), *Kphi = mat_alloc(b->n, 1);
    for (int p = 1; p <= b->pisos; p++)
        for (int c = 0; c <= b->vanos; c++)
            M_AT(phi, building_node(b, p, c), 0) = sin(PI * p / (2.0 * b->pisos + 1.0));
    spmm_csr(b->K, phi, Kphi);
    double num = 0.0, den = 0.0;
    for (int i = 0; i < b->n; i++) {
        num += M_AT(phi, i, 0) * M_AT(Kphi, i, 0);
        den += b->mass[i] * M_AT(phi, i, 0) * M_AT(phi, i, 0);
    }
    mat_free(phi); mat_free(Kphi);
    return 2.0 * PI / sqrt(num / den);
}
