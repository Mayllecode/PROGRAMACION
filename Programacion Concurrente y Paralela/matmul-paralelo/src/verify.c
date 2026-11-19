#include "verify.h"

#include <math.h>
#include <stdio.h>

VerifyResult mat_compare(const Matrix *ref, const Matrix *x, double eps) {
    VerifyResult r = {1, 0.0, 0.0, -1, -1};
    if (ref->rows != x->rows || ref->cols != x->cols) {
        r.ok = 0;
        return r;
    }
    for (int i = 0; i < ref->rows; i++)
        for (int j = 0; j < ref->cols; j++) {
            double a = M_AT(ref, i, j), b = M_AT(x, i, j);
            double d = fabs(a - b);
            if (d > r.max_abs) r.max_abs = d;
            if (fabs(a) > 0.0 && d / fabs(a) > r.max_rel) r.max_rel = d / fabs(a);
            if (!(d <= eps * (1.0 + fabs(a))) && r.ok) { /* también atrapa NaN */
                r.ok = 0;
                r.bad_i = i;
                r.bad_j = j;
            }
        }
    return r;
}

int mat_equal_exact(const Matrix *A, const Matrix *B) {
    if (A->rows != B->rows || A->cols != B->cols) return 0;
    for (size_t t = 0; t < (size_t)A->rows * (size_t)A->cols; t++)
        if (A->data[t] != B->data[t]) return 0;
    return 1;
}

void verify_report(const char *label, VerifyResult r) {
    if (r.ok)
        printf("  [OK]    %-45s err_abs=%.2e err_rel=%.2e\n", label, r.max_abs, r.max_rel);
    else if (r.bad_i < 0)
        printf("  [FALLA] %-45s dimensiones distintas\n", label);
    else
        printf("  [FALLA] %-45s primer error en (%d,%d), err_abs=%.2e\n",
               label, r.bad_i, r.bad_j, r.max_abs);
}
