#include "sim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timer.h"

double sim_dt_critical(const CSR *K, const double *mass) {
    double wmax2 = 0.0;
    for (int i = 0; i < K->rows; i++) {
        double row = 0.0;
        for (int t = K->row_ptr[i]; t < K->row_ptr[i + 1]; t++) row += fabs(K->val[t]);
        double w2 = row / mass[i];
        if (w2 > wmax2) wmax2 = w2;
    }
    return wmax2 > 0.0 ? 2.0 / sqrt(wmax2) : INFINITY;
}

static void update_peaks(const Matrix *U, const int *below, SimResult *res) {
    const int n = U->rows, k = U->cols;
    for (int i = 0; i < n; i++)
        for (int s = 0; s < k; s++) {
            double u = M_AT(U, i, s);
            if (fabs(u) > M_AT(res->peak_u, i, s)) M_AT(res->peak_u, i, s) = fabs(u);
            if (below) {
                double d = below[i] >= 0 ? u - M_AT(U, below[i], s) : u;
                if (fabs(d) > M_AT(res->peak_drift, i, s)) M_AT(res->peak_drift, i, s) = fabs(d);
            }
        }
}

int sim_run(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
            const Matrix *ag, const Matrix *u0, const Matrix *v0, SimResult *res) {
    const int n = K->rows, k = cfg->k, N = cfg->nsteps;
    const double dt = cfg->dt, a = cfg->alpha;
    if (K->rows != K->cols || k < 1 || N < 1 || dt <= 0.0 || !cfg->spmm) return -1;
    if (ag && (ag->rows < N || ag->cols != k)) return -1;
    if (u0 && (u0->rows != n || u0->cols != k)) return -1;
    if (v0 && (v0->rows != n || v0->cols != k)) return -1;
    if (cfg->record_node >= n) return -1;

    double t_start = now_sec(), t0;
    memset(res, 0, sizeof *res);
    res->peak_u = mat_alloc(n, k);
    res->peak_drift = mat_alloc(n, k);
    if (cfg->record_node >= 0) res->history = xcalloc(((size_t)N + 1) * (size_t)k, sizeof(double));

    Matrix *Up = mat_alloc(n, k);                     /* U(n-1) */
    Matrix *U = u0 ? mat_copy(u0) : mat_alloc(n, k);  /* U(n)   */
    Matrix *Un = mat_alloc(n, k);                     /* U(n+1) */
    Matrix *KU = mat_alloc(n, k);

    t0 = now_sec();
    cfg->spmm(K, U, KU);
    res->t_kernel += now_sec() - t0;

    /* Arranque: U(-1) = U(0) − Δt·V(0) + Δt²/2·A(0) */
    for (int i = 0; i < n; i++)
        for (int s = 0; s < k; s++) {
            double v = v0 ? M_AT(v0, i, s) : 0.0;
            double g = ag ? M_AT(ag, 0, s) : 0.0;
            double acc = -g - M_AT(KU, i, s) / mass[i] - a * v;
            M_AT(Up, i, s) = M_AT(U, i, s) - dt * v + 0.5 * dt * dt * acc;
        }

    const double inv_dt2 = 1.0 / (dt * dt);
    const double c1 = inv_dt2 + a / (2.0 * dt), c2 = inv_dt2 - a / (2.0 * dt);
    update_peaks(U, below, res);
    if (res->history)
        for (int s = 0; s < k; s++) res->history[s] = M_AT(U, cfg->record_node, s);

    for (int step = 0; step < N; step++) {
        for (int i = 0; i < n; i++) {
            const double inv_m = 1.0 / mass[i];
            for (int s = 0; s < k; s++) {
                double g = ag ? M_AT(ag, step, s) : 0.0;
                double rhs = 2.0 * inv_dt2 * M_AT(U, i, s) - c2 * M_AT(Up, i, s)
                             - g - M_AT(KU, i, s) * inv_m;
                M_AT(Un, i, s) = rhs / c1;
            }
        }
        Matrix *tmp = Up; Up = U; U = Un; Un = tmp;   /* rotar sin copiar */
        update_peaks(U, below, res);
        if (res->history)
            for (int s = 0; s < k; s++) res->history[(size_t)(step + 1) * k + s] = M_AT(U, cfg->record_node, s);
        if (step + 1 < N) {
            t0 = now_sec();
            cfg->spmm(K, U, KU);                      /* el producto K·U */
            res->t_kernel += now_sec() - t0;
        }
    }

    res->u = U;
    mat_free(Up); mat_free(Un); mat_free(KU);
    res->t_total = now_sec() - t_start;
    return 0;
}

void sim_result_free(SimResult *res) {
    mat_free(res->u);
    mat_free(res->peak_u);
    mat_free(res->peak_drift);
    free(res->history);
    memset(res, 0, sizeof *res);
}
