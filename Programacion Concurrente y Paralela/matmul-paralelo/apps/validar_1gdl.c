/*
 * validar_1gdl.c — Evidencia del Hito H1 en forma de datos para graficar.
 *
 * Simula un oscilador de 1 grado de libertad (m = 1000 kg, k = 4e5 N/m,
 * T = 2π·sqrt(m/k) = 0.3142 s) que se suelta desde u0 = 1 y compara con la
 * solución exacta. Es el mismo caso que tests/test_sim.c, pero guardando las
 * curvas en vez de solo decir OK/FALLA:
 *
 *   results/validacion_1gdl.csv     t, simulado y exacto (sin y con amortiguamiento)
 *   results/convergencia_1gdl.csv   error máximo para Δt cada vez más chico
 *
 * Uso: ./build/validar_1gdl
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "seq.h"
#include "sim.h"

#define PI 3.14159265358979323846

static const double M = 1000.0, KK = 4e5;

/* Vibración libre desde u0 = 1; devuelve la historia (N + 1 valores, hay que liberarla) */
static double *simulate(double alpha, double dt, int N) {
    CSR *K = csr_alloc(1, 1, 1);
    K->row_ptr[1] = 1; K->col_idx[0] = 0; K->val[0] = KK;
    Matrix *u0 = mat_alloc(1, 1);
    M_AT(u0, 0, 0) = 1.0;
    SimConfig cfg = {dt, N, 1, alpha, spmm_csr, 0};
    SimResult r;
    sim_run(K, &M, NULL, &cfg, NULL, u0, NULL, &r);
    double *h = r.history;
    r.history = NULL;                     /* nos quedamos con la historia */
    sim_result_free(&r);
    csr_free(K); mat_free(u0);
    return h;
}

static double exact(double alpha, double t) {
    double w = sqrt(KK / M), wd = sqrt(w * w - alpha * alpha / 4.0);
    return exp(-alpha * t / 2.0) * (cos(wd * t) + alpha / (2.0 * wd) * sin(wd * t));
}

static double max_error(double alpha, double w_dt, double periods) {
    double w = sqrt(KK / M), dt = w_dt / w;
    int N = (int)(periods * 2.0 * PI / w_dt);
    double *h = simulate(alpha, dt, N), err = 0.0;
    for (int s = 0; s <= N; s++) err = fmax(err, fabs(h[s] - exact(alpha, s * dt)));
    free(h);
    return err;
}

int main(void) {
    const double w = sqrt(KK / M), T = 2.0 * PI / w, alpha = 2.0;
    const double w_dt = 0.05, periods = 5.0;     /* Δt = T/126: curva suave y error visible */
    const double dt = w_dt / w;
    const int N = (int)(periods * 2.0 * PI / w_dt);

    printf("Oscilador de 1 GDL: m = %.0f kg, k = %.0e N/m -> T = 2*pi*sqrt(m/k) = %.6f s\n", M, KK, T);
    double *h0 = simulate(0.0, dt, N), *h1 = simulate(alpha, dt, N);
    FILE *f = fopen("results/validacion_1gdl.csv", "w");
    if (!f) { fprintf(stderr, "No se pudo escribir results/validacion_1gdl.csv (¿existe results/?)\n"); return 1; }
    fprintf(f, "t_s,u_sim,u_exacta,u_sim_amort,u_exacta_amort\n");
    double e0 = 0.0, e1 = 0.0;
    for (int s = 0; s <= N; s++) {
        double t = s * dt, x0 = exact(0.0, t), x1 = exact(alpha, t);
        e0 = fmax(e0, fabs(h0[s] - x0));
        e1 = fmax(e1, fabs(h1[s] - x1));
        fprintf(f, "%.6f,%.9e,%.9e,%.9e,%.9e\n", t, h0[s], x0, h1[s], x1);
    }
    fclose(f);
    printf("Curvas con dt = T/%.0f, %d pasos: error max %.2e (sin amortiguar), %.2e (alpha = %.0f 1/s)\n",
           T / dt, N, e0, e1, alpha);
    free(h0); free(h1);

    /* Convergencia: el método es de segundo orden, el error cae 4x al dividir Δt entre 2 */
    f = fopen("results/convergencia_1gdl.csv", "w");
    if (!f) { fprintf(stderr, "No se pudo escribir results/convergencia_1gdl.csv\n"); return 1; }
    fprintf(f, "dt_s,omega_dt,error_max,razon\n");
    printf("\n%10s %10s %12s %8s\n", "dt [s]", "w*dt", "error max", "razon");
    double prev = 0.0;
    for (double wd = 0.2; wd > 0.003; wd /= 2.0) {
        double e = max_error(0.0, wd, periods);
        double ratio = prev > 0.0 ? prev / e : 0.0;
        fprintf(f, "%.6e,%.6f,%.6e,%.4f\n", wd / w, wd, e, ratio);
        if (prev > 0.0) printf("%10.2e %10.4f %12.3e %8.2f\n", wd / w, wd, e, ratio);
        else            printf("%10.2e %10.4f %12.3e %8s\n", wd / w, wd, e, "-");
        prev = e;
    }
    fclose(f);
    printf("\nRazon ~4 en cada fila = metodo de segundo orden, como dice la teoria.\n");
    printf("Archivos: results/validacion_1gdl.csv y results/convergencia_1gdl.csv\n");
    return 0;
}
