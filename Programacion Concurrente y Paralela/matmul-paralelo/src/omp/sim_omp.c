/*
 * sim_omp.c — El simulador sísmico con OpenMP (semana 7).
 *
 * Es sim_run (src/sim.c) línea por línea, con el trabajo de cada paso
 * repartido entre hilos. Por qué no basta con paralelizar K·U:
 *
 *   bench_sim midió que K·U es ~47 % del tiempo del simulador. Por la ley
 *   de Amdahl, si solo eso va en paralelo, el speedup no pasa de
 *   1 / (1 − 0.47) ≈ 1.9×, por muchos hilos que haya. El otro ~53 % es la
 *   actualización de U y el cálculo de los picos, y también se reparte.
 *
 * Un paso de tiempo queda así:
 *
 *   1. K·U(n)            -> cfg->spmm (spmm_csr_omp: filas entre hilos)
 *   2. U(n+1) por nodo   -> "omp for": cada hilo actualiza SUS nodos
 *      --- barrera implícita: todos terminan U(n+1) antes de seguir ---
 *   3. picos por nodo    -> "omp for": la deriva lee el nodo de ABAJO, que
 *                           puede ser de otro hilo; por eso va después de
 *                           la barrera
 *   4. rotar U(n-1), U(n), U(n+1) y guardar la historia: un solo hilo
 *
 * Como cada valor se calcula con las mismas operaciones y en el mismo orden
 * que en sim_run, el resultado es idéntico bit a bit (tests/test_omp.c).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "par_omp.h"
#include "timer.h"

/* Picos del nodo i: desplazamiento |u| y deriva |u_i − u_abajo|.
 * Se llama desde dentro de un "omp for": cada hilo, con sus nodos. */
static void peaks_row(const Matrix *U, const int *below, SimResult *res, int i) {
    const int k = U->cols;
    for (int s = 0; s < k; s++) {
        double u = M_AT(U, i, s);
        if (fabs(u) > M_AT(res->peak_u, i, s)) M_AT(res->peak_u, i, s) = fabs(u);
        if (below) {
            double d = below[i] >= 0 ? u - M_AT(U, below[i], s) : u;
            if (fabs(d) > M_AT(res->peak_drift, i, s)) M_AT(res->peak_drift, i, s) = fabs(d);
        }
    }
}

int sim_run_omp(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
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

    /* Arranque: U(-1) = U(0) − Δt·V(0) + Δt²/2·A(0), y picos de U(0) */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        for (int s = 0; s < k; s++) {
            double v = v0 ? M_AT(v0, i, s) : 0.0;
            double g = ag ? M_AT(ag, 0, s) : 0.0;
            double acc = -g - M_AT(KU, i, s) / mass[i] - a * v;
            M_AT(Up, i, s) = M_AT(U, i, s) - dt * v + 0.5 * dt * dt * acc;
        }
        peaks_row(U, below, res, i);   /* U(0) no cambia aquí: no hay carrera */
    }

    const double inv_dt2 = 1.0 / (dt * dt);
    const double c1 = inv_dt2 + a / (2.0 * dt), c2 = inv_dt2 - a / (2.0 * dt);
    if (res->history)
        for (int s = 0; s < k; s++) res->history[s] = M_AT(U, cfg->record_node, s);

    for (int step = 0; step < N; step++) {
        /* Una sola región paralela por paso con dos repartos de nodos */
        #pragma omp parallel
        {
            #pragma omp for schedule(static)
            for (int i = 0; i < n; i++) {
                const double inv_m = 1.0 / mass[i];
                for (int s = 0; s < k; s++) {
                    double g = ag ? M_AT(ag, step, s) : 0.0;
                    double rhs = 2.0 * inv_dt2 * M_AT(U, i, s) - c2 * M_AT(Up, i, s)
                                 - g - M_AT(KU, i, s) * inv_m;
                    M_AT(Un, i, s) = rhs / c1;
                }
            }
            /* (barrera implícita: U(n+1) está completo antes de los picos) */
            #pragma omp for schedule(static)
            for (int i = 0; i < n; i++) peaks_row(Un, below, res, i);
        }
        Matrix *tmp = Up; Up = U; U = Un; Un = tmp;   /* rotar sin copiar */
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
