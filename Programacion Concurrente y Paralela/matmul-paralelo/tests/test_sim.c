/*
 * test_sim.c — Validación del modelo físico y del simulador (Hito H1).
 *
 * Idea: antes de paralelizar, comprobar que el simulador resuelve bien la
 * física en casos cuya respuesta se conoce EXACTAMENTE:
 *   1. Oscilador de 1 grado de libertad sin amortiguar: u(t) = cos(ωt).
 *   2. El mismo con amortiguamiento: solución analítica conocida.
 *   3. Orden de convergencia: al dividir Δt entre 2, el error cae ~4 veces
 *      (el método de diferencias centrales es de segundo orden).
 *   4. Sismo constante y mucho amortiguamiento: el edificio queda en el
 *      equilibrio estático K·u = −M·1·a.
 *   5. Linealidad: el sismo A+B da la respuesta de A más la de B.
 *   6. Varios escenarios a la vez dan lo mismo que uno por uno.
 *   7. Modelo del edificio: simetría, filas que suman lo esperado y
 *      periodo fundamental igual a la fórmula cerrada.
 *   8. Criterio literal del Hito H1: el periodo MEDIDO en la vibración
 *      libre simulada es T = 2π·sqrt(m/k).
 *   9. Lectura de acelerogramas reales: cabecera, columnas, unidades.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "building.h"
#include "generators.h"
#include "ground_motion.h"
#include "seq.h"
#include "sim.h"
#include "verify.h"

#define PI 3.14159265358979323846

static int n_ok = 0, n_fail = 0;
static void check(int cond, const char *msg) {
    if (cond) { n_ok++;   printf("  [OK]    %s\n", msg); }
    else      { n_fail++; printf("  [FALLA] %s\n", msg); }
}

/* Corre un oscilador de 1 GDL (m, kk) con u0 = 1, v0 = 0 y devuelve el
 * error máximo contra la solución analítica durante `periods` periodos. */
static double sdof_error(double m, double kk, double alpha, double w_dt, double periods) {
    double w = sqrt(kk / m), dt = w_dt / w;
    int N = (int)(periods * 2.0 * PI / w / dt);
    CSR *K = csr_alloc(1, 1, 1);
    K->row_ptr[1] = 1; K->col_idx[0] = 0; K->val[0] = kk;
    Matrix *u0 = mat_alloc(1, 1);
    M_AT(u0, 0, 0) = 1.0;
    SimConfig cfg = {dt, N, 1, alpha, spmm_csr, 0};
    SimResult r;
    sim_run(K, &m, NULL, &cfg, NULL, u0, NULL, &r);
    double wd = sqrt(w * w - alpha * alpha / 4.0), err = 0.0;
    for (int s = 0; s <= N; s++) {
        double t = s * dt;
        double exact = exp(-alpha * t / 2.0) * (cos(wd * t) + alpha / (2.0 * wd) * sin(wd * t));
        double e = fabs(r.history[s] - exact);
        if (e > err) err = e;
    }
    sim_result_free(&r);
    csr_free(K); mat_free(u0);
    return err;
}

static void test_sdof(void) {
    printf("\n1) Oscilador de 1 GDL (m = 1000 kg, k = 4e5 N/m -> T = %.4f s)\n",
           2.0 * PI / sqrt(400.0));
    double e = sdof_error(1000.0, 4e5, 0.0, 0.01, 5.0);
    char msg[128];
    snprintf(msg, sizeof msg, "sin amortiguar, 5 periodos: error max %.2e < 1e-3", e);
    check(e < 1e-3, msg);

    double ed = sdof_error(1000.0, 4e5, 2.0, 0.01, 5.0);
    snprintf(msg, sizeof msg, "con amortiguamiento (alpha = 2): error max %.2e < 1e-3", ed);
    check(ed < 1e-3, msg);

    double e1 = sdof_error(1000.0, 4e5, 0.0, 0.02, 5.0);
    double e2 = sdof_error(1000.0, 4e5, 0.0, 0.01, 5.0);
    double ratio = e1 / e2;
    snprintf(msg, sizeof msg, "segundo orden: error(dt)/error(dt/2) = %.2f (esperado ~4)", ratio);
    check(ratio > 3.5 && ratio < 4.5, msg);

    CSR *K = csr_alloc(1, 1, 1);
    K->row_ptr[1] = 1; K->val[0] = 4e5;
    double m = 1000.0;
    snprintf(msg, sizeof msg, "paso critico 1 GDL = 2/omega = %.5f s", sim_dt_critical(K, &m));
    check(fabs(sim_dt_critical(K, &m) - 2.0 / 20.0) < 1e-12, msg);
    csr_free(K);
}

/* Criterio del Hito H1: se simula la vibración libre y se MIDE el periodo
 * en la respuesta (tiempo entre cruces por cero de subida, interpolados),
 * sin usar la solución analítica. */
static void test_period_h1(void) {
    const double m = 1000.0, kk = 4e5, T = 2.0 * PI * sqrt(m / kk);
    printf("\n1b) Hito H1: periodo medido en la simulacion vs T = 2*pi*sqrt(m/k) = %.6f s\n", T);
    const double dt = T / 400.0;
    const int N = (int)(20.0 * T / dt);
    CSR *K = csr_alloc(1, 1, 1);
    K->row_ptr[1] = 1; K->col_idx[0] = 0; K->val[0] = kk;
    Matrix *u0 = mat_alloc(1, 1);
    M_AT(u0, 0, 0) = 0.05;                        /* 5 cm iniciales, se suelta */
    SimConfig cfg = {dt, N, 1, 0.0, spmm_csr, 0};
    SimResult r;
    sim_run(K, &m, NULL, &cfg, NULL, u0, NULL, &r);
    double t_first = -1.0, t_last = -1.0;
    int crossings = 0;
    for (int s = 0; s < N; s++) {
        double a = r.history[s], c = r.history[s + 1];
        if (a < 0.0 && c >= 0.0) {
            double t = (s + (-a) / (c - a)) * dt;
            if (crossings == 0) t_first = t;
            t_last = t;
            crossings++;
        }
    }
    double T_med = crossings > 1 ? (t_last - t_first) / (crossings - 1) : 0.0;
    double err = fabs(T_med - T) / T;
    char msg[160];
    snprintf(msg, sizeof msg, "periodo medido en %d ciclos = %.6f s, error relativo %.1e < 1e-4",
             crossings - 1, T_med, err);
    check(crossings > 1 && err < 1e-4, msg);
    sim_result_free(&r);
    csr_free(K); mat_free(u0);
}

static void test_building_model(void) {
    printf("\n2) Modelo del edificio (4 pisos, 3 vanos)\n");
    Building *b = building_create(4, 3, 2e4, 3e7, 1e8, 3.0);
    Matrix *Kd = csr_to_dense(b->K), *Kt = mat_transpose(Kd);
    check(b->n == 16, "16 grados de libertad (4 pisos x 4 lineas de columnas)");
    check(mat_equal_exact(Kd, Kt), "K es simetrica");
    /* K·1: solo los nodos del piso 1 sienten la columna que va al suelo */
    int ok = 1;
    for (int p = 1; p <= 4; p++)
        for (int c = 0; c <= 3; c++) {
            int i = building_node(b, p, c);
            double row = 0.0;
            for (int j = 0; j < b->n; j++) row += M_AT(Kd, i, j);
            double expected = (p == 1) ? 3e7 : 0.0;
            if (fabs(row - expected) > 1e-6) ok = 0;
        }
    check(ok, "K·1 = kc en el piso 1 y 0 en los demas (equilibrio de resortes)");
    check(b->below[building_node(b, 1, 2)] == -1 && b->below[building_node(b, 3, 2)] == building_node(b, 2, 2),
          "nodo de abajo bien enlazado (suelo para el piso 1)");
    mat_free(Kd); mat_free(Kt); building_free(b);

    /* Periodo fundamental contra la fórmula cerrada del edificio de cortante
     * uniforme: ω1 = 2·sqrt(kc/m)·sin(π / (2·(2N + 1))) */
    const double m = 2e4, kc = 3.5e7;
    Building *b10 = building_create(10, 4, m, kc, 1e8, 3.0);
    double T_exact = 2.0 * PI / (2.0 * sqrt(kc / m) * sin(PI / (2.0 * (2.0 * 10 + 1.0))));
    double T_num = building_fundamental_period(b10);
    char msg[160];
    snprintf(msg, sizeof msg, "T1 (10 pisos, Rayleigh) = %.6f s == formula cerrada %.6f s", T_num, T_exact);
    check(fabs(T_num - T_exact) / T_exact < 1e-12, msg);
    building_free(b10);

    Building *b1 = building_create(1, 0, m, kc, 1e8, 3.0);
    T_exact = 2.0 * PI * sqrt(m / kc);
    snprintf(msg, sizeof msg, "T1 (1 piso, 1 columna) == 2*pi*sqrt(m/kc) = %.6f s", T_exact);
    check(fabs(building_fundamental_period(b1) - T_exact) / T_exact < 1e-12, msg);
    building_free(b1);
}

static void test_static_equilibrium(void) {
    printf("\n3) Sismo constante + mucho amortiguamiento -> equilibrio estatico\n");
    Building *b = building_create(3, 2, 2e4, 3e7, 1e8, 3.0);
    double dtc = sim_dt_critical(b->K, b->mass), dt = 0.5 * dtc;
    int N = (int)(20.0 / dt);
    Matrix *ag = mat_alloc(N, 1);
    for (int s = 0; s < N; s++) M_AT(ag, s, 0) = 2.0; /* 2 m/s^2 constantes */
    SimConfig cfg = {dt, N, 1, 60.0, spmm_csr, -1};
    SimResult r;
    sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r);
    /* residuo: K·u + M·1·a = 0 */
    Matrix *Ku = mat_alloc(b->n, 1);
    spmm_csr(b->K, r.u, Ku);
    double res = 0.0, scale = 0.0;
    for (int i = 0; i < b->n; i++) {
        res = fmax(res, fabs(M_AT(Ku, i, 0) + b->mass[i] * 2.0));
        scale = fmax(scale, b->mass[i] * 2.0);
    }
    char msg[128];
    snprintf(msg, sizeof msg, "residuo relativo |K·u + M·a| / |M·a| = %.2e < 1e-6", res / scale);
    check(res / scale < 1e-6, msg);
    check(M_AT(r.u, 0, 0) < 0.0, "el edificio se desplaza en sentido contrario a la aceleracion del suelo");
    sim_result_free(&r); mat_free(ag); mat_free(Ku); building_free(b);
}

static void test_linearity_and_batching(void) {
    printf("\n4) Linealidad y varios escenarios a la vez\n");
    Building *b = building_create(5, 2, 2e4, 3e7, 1e8, 3.0);
    double dt = 0.5 * sim_dt_critical(b->K, b->mass);
    int N = (int)(8.0 / dt);
    double *a1 = malloc(N * sizeof(double)), *a2 = malloc(N * sizeof(double));
    ground_motion_synthetic(a1, N, dt, 2.0, 1.0);
    ground_motion_synthetic(a2, N, dt, 1.0, 3.0);
    Matrix *ag = mat_alloc(N, 3);
    for (int s = 0; s < N; s++) {
        M_AT(ag, s, 0) = a1[s];
        M_AT(ag, s, 1) = a2[s];
        M_AT(ag, s, 2) = a1[s] + a2[s];
    }
    SimConfig cfg = {dt, N, 3, 0.6, spmm_csr, -1};
    SimResult r;
    sim_run(b->K, b->mass, b->below, &cfg, ag, NULL, NULL, &r);
    Matrix *sum = mat_alloc(b->n, 1), *third = mat_alloc(b->n, 1);
    for (int i = 0; i < b->n; i++) {
        M_AT(sum, i, 0) = M_AT(r.u, i, 0) + M_AT(r.u, i, 1);
        M_AT(third, i, 0) = M_AT(r.u, i, 2);
    }
    VerifyResult v = mat_compare(third, sum, VERIFY_EPS);
    verify_report("respuesta(A+B) == respuesta(A) + respuesta(B)", v);
    if (v.ok) n_ok++; else n_fail++;

    /* El escenario 1 corrido solo debe dar exactamente lo mismo */
    Matrix *ag1 = mat_alloc(N, 1);
    for (int s = 0; s < N; s++) M_AT(ag1, s, 0) = a2[s];
    SimConfig cfg1 = cfg; cfg1.k = 1;
    SimResult r1;
    sim_run(b->K, b->mass, b->below, &cfg1, ag1, NULL, NULL, &r1);
    int same = 1;
    for (int i = 0; i < b->n; i++)
        if (M_AT(r1.u, i, 0) != M_AT(r.u, i, 1) || M_AT(r1.peak_u, i, 0) != M_AT(r.peak_u, i, 1)) same = 0;
    check(same, "3 escenarios juntos == cada escenario por separado (exacto)");

    /* Deriva: en el piso 1 la deriva maxima es el desplazamiento maximo */
    int ok = 1;
    for (int c = 0; c <= b->vanos; c++) {
        int i = building_node(b, 1, c);
        if (M_AT(r.peak_drift, i, 0) != M_AT(r.peak_u, i, 0)) ok = 0;
    }
    check(ok, "deriva del piso 1 = desplazamiento del piso 1 (la base no se mueve)");

    sim_result_free(&r); sim_result_free(&r1);
    mat_free(ag); mat_free(ag1); mat_free(sum); mat_free(third);
    free(a1); free(a2); building_free(b);
}

static void test_stability(void) {
    printf("\n5) Estabilidad con el paso critico\n");
    Building *b = building_create(6, 3, 2e4, 3e7, 1e8, 3.0);
    double dtc = sim_dt_critical(b->K, b->mass);
    Matrix *u0 = gen_random(b->n, 1, 77);
    SimConfig cfg = {0.95 * dtc, 20000, 1, 0.0, spmm_csr, -1};
    SimResult r;
    sim_run(b->K, b->mass, b->below, &cfg, NULL, u0, NULL, &r);
    double mx = 0.0;
    for (int i = 0; i < b->n; i++) mx = fmax(mx, M_AT(r.peak_u, i, 0));
    char msg[128];
    snprintf(msg, sizeof msg, "con dt = 0.95·dt_crit (%.2e s), 20 000 pasos: max |u| = %.2f (acotado)", cfg.dt, mx);
    check(isfinite(mx) && mx < 10.0, msg);
    sim_result_free(&r); mat_free(u0); building_free(b);
}

static void test_record_loader(void) {
    printf("\n6) Lectura de acelerogramas (cabecera, columnas, unidades)\n");
    const char *path = "test_registro_tmp.txt";
    FILE *f = fopen(path, "w");
    if (!f) { check(0, "no se pudo crear el archivo temporal de prueba"); return; }
    /* Cabecera con números (fecha, dt) que NO deben leerse como aceleraciones;
     * columnas: tiempo, EW [gal], NS [gal]; una línea vacía en medio */
    fprintf(f, "# comentario\n"
               "ESTACION: PRUEBA   FECHA: 2007/08/15 18:40\n"
               "UNIDADES: cm/s2  DT: 0.01\n"
               "t EW NS\n"
               "0.00 0.0 10.0\n"
               "0.01 100.0 20.0\n"
               "\n"
               "0.02 -50.0 30.0\n"
               "0.03 0.0 40.0\n");
    fclose(f);

    double out[9];
    RecordInfo ri;
    int got = ground_motion_load(path, 2, 0.01, 0.01, 0.005, 9, out, &ri);
    check(got == 4 && ri.samples == 4 && ri.header_lines == 4 && ri.columns == 3,
          "cabecera ignorada: 4 muestras, 4 lineas de cabecera, 3 columnas");
    /* columna EW en m/s^2: 0, 1, -0.5, 0; re-muestreada a dt/2 */
    const double expected[9] = {0.0, 0.5, 1.0, 0.25, -0.5, -0.25, 0.0, 0.0, 0.0};
    int ok = got == 4;
    for (int s = 0; s < 9 && ok; s++)
        if (fabs(out[s] - expected[s]) > 1e-15) ok = 0;
    check(ok, "columna 2 en gal -> m/s^2 y re-muestreo lineal a dt/2 (exacto)");
    check(fabs(ri.pga - 1.0) < 1e-15, "PGA del registro = 1.0 m/s^2");
    check(ground_motion_load(path, 4, 0.01, 0.01, 0.01, 4, out, &ri) == -2 && ri.columns == 3,
          "columna inexistente (4 de 3) se rechaza (devuelve -2)");

    /* Varios valores por línea, en fracción de g (tipo PEER .AT2), con un
     * exponente Fortran (D) */
    f = fopen(path, "w");
    fprintf(f, "PEER STRONG MOTION DATABASE RECORD\n"
               "PRUEBA 2026\n"
               "ACCELERATION TIME SERIES IN UNITS OF G\n"
               "NPTS=    6, DT=   .0100 SEC\n"
               "  .1000E-01  .2000E-01 -.3000E-01\n"
               "  .4000D-01  .5000E-01  .6000E-01\n");
    fclose(f);
    double out2[6];
    got = ground_motion_load(path, 0, 0.01, G_ACCEL, 0.01, 6, out2, &ri);
    ok = got == 6 && ri.header_lines == 4;
    for (int s = 0; s < 6 && ok; s++) {
        double v = (s == 2 ? -1.0 : 1.0) * 0.01 * (s + 1) * G_ACCEL;
        if (fabs(out2[s] - v) > 1e-15) ok = 0;
    }
    check(ok, "varios valores por linea (tipo PEER, exponente D): 6 muestras en orden, g -> m/s^2");
    remove(path);
    check(ground_motion_load("no_existe_este_archivo.txt", 0, 0.01, 1.0, 0.01, 1, out, NULL) == -1,
          "archivo inexistente devuelve -1");
}

int main(void) {
    printf("=== Validacion del simulador sismico (version secuencial) ===\n");
    test_sdof();
    test_period_h1();
    test_building_model();
    test_static_equilibrium();
    test_linearity_and_batching();
    test_stability();
    test_record_loader();
    printf("\n=== Resultado: %d pruebas OK, %d fallas ===\n", n_ok, n_fail);
    return n_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
