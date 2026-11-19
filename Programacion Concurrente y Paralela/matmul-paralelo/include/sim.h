/*
 * sim.h — Simulador de la respuesta sísmica (diferencias centrales).
 *
 * Resuelve, para k escenarios a la vez (una columna de U por escenario):
 *
 *     M·Ü + α·M·U' + K·U = −M·1·a_g(t)
 *
 * U = desplazamientos relativos al suelo (n x k), M = masas (diagonal),
 * α = amortiguamiento proporcional a la masa, a_g = aceleración del suelo.
 *
 * Actualización explícita en cada paso:
 *     c1·U(n+1) = (2/Δt²)·U(n) − c2·U(n−1) − a_g(n) − M⁻¹·K·U(n)
 *     c1 = 1/Δt² + α/(2Δt),   c2 = 1/Δt² − α/(2Δt)
 *
 * Como M es diagonal, el ÚNICO trabajo pesado por paso es el producto
 * K·U(n). Ese producto se hace con la función `spmm` de la configuración:
 * spmm_csr en secuencial, y en las siguientes fases la versión OpenMP, MPI
 * o CUDA, sin cambiar nada más del simulador.
 *
 * Estabilidad: el método explícito exige Δt < 2/ω_max. sim_dt_critical da
 * una cota segura (por el teorema de Gershgorin).
 */
#ifndef SIM_H
#define SIM_H

#include "matrix.h"

typedef int (*spmm_fn)(const CSR *A, const Matrix *B, Matrix *C);

typedef struct {
    double dt;        /* paso de tiempo [s] */
    int nsteps;       /* número de pasos */
    int k;            /* número de escenarios (columnas de U) */
    double alpha;     /* amortiguamiento: C = alpha·M [1/s]; 0 = sin amortiguar */
    spmm_fn spmm;     /* kernel K·U */
    int record_node;  /* nodo cuyo desplazamiento se guarda en cada paso (todos los escenarios); -1 = ninguno */
} SimConfig;

typedef struct {
    Matrix *u;           /* desplazamiento al final, n x k [m] */
    Matrix *peak_u;      /* máximo |u| en el tiempo, por nodo y escenario [m] */
    Matrix *peak_drift;  /* máximo |u_i − u_abajo(i)| por nodo y escenario [m] */
    double *history;     /* u(record_node, s) en los pasos 0..nsteps: history[paso * k + s]; o NULL */
    double t_kernel;     /* segundos dentro del producto K·U */
    double t_total;      /* segundos de la simulación completa */
} SimResult;

/* ag:  aceleración del suelo, nsteps x k (fila = paso, columna = escenario),
 *      en m/s^2. NULL = vibración libre.
 * u0, v0: condiciones iniciales n x k; NULL = reposo.
 * below: nodo de abajo de cada nodo (-1 = suelo) para la deriva; NULL = no calcularla.
 * Devuelve 0 si todo va bien, -1 si los datos no son consistentes. */
int sim_run(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
            const Matrix *ag, const Matrix *u0, const Matrix *v0, SimResult *res);

void sim_result_free(SimResult *res);

/* Cota segura del paso crítico: 2 / ω_max, con ω_max² <= max_i Σ_j |K_ij| / m_i */
double sim_dt_critical(const CSR *K, const double *mass);

#endif
