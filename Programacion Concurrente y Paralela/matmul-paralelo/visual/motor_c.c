/*
 * motor_c.c — Motores Secuencial y OpenMP del programa visual.
 *
 * Un paso de tiempo es exactamente el de sim_run / sim_run_omp:
 *   1. U(n+1) de cada nodo con K·U(n)        (el bucle de "mover")
 *   2. rotar U(n-1), U(n), U(n+1)
 *   3. picos de desplazamiento y deriva
 *   4. K·U(n+1) para el paso siguiente        (spmm_csr o spmm_csr_omp)
 */
#include "motor_c.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#include "ground_motion.h"
#include "par_omp.h"
#include "seq.h"
#include "sim.h"
#include "timer.h"

#define PI 3.14159265358979323846

static int g_hilos = 1;   /* hilos de OpenMP (ver omp_hilos) */

Esc *esc_crear(int pisos, int vanos, int k, double duracion) {
    /* Mismos parámetros que apps/demo_sismo.c y bench/bench_sim_omp.c */
    const double m = 2.0e4, kc = 3.5e7, kb = 1.0e8, h = 3.0, zeta = 0.05;
    Esc *e = xcalloc(1, sizeof *e);
    e->pisos = pisos; e->vanos = vanos; e->k = k; e->h = h;
    e->b = building_create(pisos, vanos, m, kc, kb, h);
    e->n = e->b->n;
    e->T1 = building_fundamental_period(e->b);
    e->alpha = 2.0 * zeta * (2.0 * PI / e->T1);
    e->dt = 0.5 * sim_dt_critical(e->b->K, e->b->mass);
    e->nsteps = (int)(duracion / e->dt);
    e->amp = 0.3 * G_ACCEL;
    e->freqs = xcalloc((size_t)k, sizeof(double));
    e->ag = mat_alloc(e->nsteps, k);
    double *col = xcalloc((size_t)e->nsteps, sizeof(double));
    for (int s = 0; s < k; s++) {   /* k sismos sintéticos de 0.5 a 4 Hz, como bench_sim_omp */
        e->freqs[s] = 0.5 + 3.5 * s / (k > 1 ? k - 1 : 1);
        ground_motion_synthetic(col, e->nsteps, e->dt, e->amp, e->freqs[s]);
        for (int t = 0; t < e->nsteps; t++) M_AT(e->ag, t, s) = col[t];
    }
    free(col);
    return e;
}

void esc_liberar(Esc *e) {
    if (!e) return;
    building_free(e->b);
    mat_free(e->ag);
    free(e->freqs);
    free(e);
}

static void ku(MotorC *m) {
    if (m->paralelo) spmm_csr_omp(m->e->b->K, m->U, m->KU);
    else spmm_csr(m->e->b->K, m->U, m->KU);
}

static void picos_fila(MotorC *m, const Matrix *U, int i) {
    const int k = U->cols;
    const int *below = m->e->b->below;
    for (int s = 0; s < k; s++) {
        double u = M_AT(U, i, s);
        if (fabs(u) > M_AT(m->peak_u, i, s)) M_AT(m->peak_u, i, s) = fabs(u);
        double d = below[i] >= 0 ? u - M_AT(U, below[i], s) : u;
        if (fabs(d) > M_AT(m->peak_drift, i, s)) M_AT(m->peak_drift, i, s) = fabs(d);
    }
}

MotorC *mc_crear(const Esc *e, int paralelo) {
    MotorC *m = xcalloc(1, sizeof *m);
    m->e = e; m->paralelo = paralelo;
    m->Up = mat_alloc(e->n, e->k); m->U = mat_alloc(e->n, e->k);
    m->Un = mat_alloc(e->n, e->k); m->KU = mat_alloc(e->n, e->k);
    m->peak_u = mat_alloc(e->n, e->k); m->peak_drift = mat_alloc(e->n, e->k);
    mc_reiniciar(m);
    return m;
}

/* Edificio en reposo y arranque U(-1) = U(0) − Δt·V(0) + Δt²/2·A(0), como sim_run */
void mc_reiniciar(MotorC *m) {
    const Esc *e = m->e;
    const int n = e->n, k = e->k;
    const double dt = e->dt, a = e->alpha;
    omp_set_num_threads(g_hilos);
    mat_zero(m->Up); mat_zero(m->U); mat_zero(m->Un); mat_zero(m->KU);
    mat_zero(m->peak_u); mat_zero(m->peak_drift);
    m->paso = 0;
    ku(m);
    for (int i = 0; i < n; i++)
        for (int s = 0; s < k; s++) {
            double v = 0.0;
            double g = M_AT(e->ag, 0, s);
            double acc = -g - M_AT(m->KU, i, s) / e->b->mass[i] - a * v;
            M_AT(m->Up, i, s) = M_AT(m->U, i, s) - dt * v + 0.5 * dt * dt * acc;
        }
}

double mc_avanzar(MotorC *m, int pasos) {
    const Esc *e = m->e;
    const int n = e->n, k = e->k, N = e->nsteps;
    const double dt = e->dt, a = e->alpha;
    const double inv_dt2 = 1.0 / (dt * dt);
    const double c1 = inv_dt2 + a / (2.0 * dt), c2 = inv_dt2 - a / (2.0 * dt);
    const double *mass = e->b->mass;
    omp_set_num_threads(g_hilos);
    double t0 = now_sec();

    for (int r = 0; r < pasos && m->paso < N; r++) {
        const int step = m->paso;
        Matrix *U = m->U, *Up = m->Up, *Un = m->Un, *KU = m->KU;
        if (!m->paralelo) {
            /* ---- Secuencial: un solo núcleo recorre los nodos uno por uno ---- */
            for (int i = 0; i < n; i++) {
                const double inv_m = 1.0 / mass[i];
                for (int s = 0; s < k; s++) {
                    double g = M_AT(e->ag, step, s);
                    double rhs = 2.0 * inv_dt2 * M_AT(U, i, s) - c2 * M_AT(Up, i, s)
                                 - g - M_AT(KU, i, s) * inv_m;
                    M_AT(Un, i, s) = rhs / c1;
                }
            }
            for (int i = 0; i < n; i++) picos_fila(m, Un, i);
        } else {
            /* ---- OpenMP: los nodos se reparten entre los hilos ---- */
            #pragma omp parallel
            {
                #pragma omp for schedule(static)
                for (int i = 0; i < n; i++) {
                    const double inv_m = 1.0 / mass[i];
                    for (int s = 0; s < k; s++) {
                        double g = M_AT(e->ag, step, s);
                        double rhs = 2.0 * inv_dt2 * M_AT(U, i, s) - c2 * M_AT(Up, i, s)
                                     - g - M_AT(KU, i, s) * inv_m;
                        M_AT(Un, i, s) = rhs / c1;
                    }
                }
                /* barrera implícita: U(n+1) completo antes de los picos */
                #pragma omp for schedule(static)
                for (int i = 0; i < n; i++) picos_fila(m, Un, i);
            }
        }
        m->Up = U; m->U = Un; m->Un = Up;   /* rotar sin copiar */
        m->paso++;
        if (m->paso < N) ku(m);              /* el producto K·U del paso siguiente */
    }
    return now_sec() - t0;
}

void mc_liberar(MotorC *m) {
    if (!m) return;
    mat_free(m->Up); mat_free(m->U); mat_free(m->Un); mat_free(m->KU);
    mat_free(m->peak_u); mat_free(m->peak_drift);
    free(m);
}

/* omp_set_num_threads vale solo para el hilo del sistema que la llama, y Go
 * puede llamar a C desde distintos hilos: por eso se guarda aquí y se
 * vuelve a fijar al entrar a cada función que usa OpenMP. */

void omp_hilos(int p) { g_hilos = p; omp_set_num_threads(p); }

int nucleos(void) { return omp_get_num_procs(); }

void duenos_omp(int n, int *out) {
    omp_set_num_threads(g_hilos);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) out[i] = omp_get_thread_num();
}
