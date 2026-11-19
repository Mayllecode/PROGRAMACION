/*
 * verify.h — Verificación de correctitud contra la referencia.
 *
 * Criterio: el elemento x es correcto si  |x - ref| <= eps * (1 + |ref|).
 *   - Para valores pequeños actúa como tolerancia absoluta (eps).
 *   - Para valores grandes actúa como tolerancia relativa (eps * |ref|).
 *
 * ¿Por qué no exigir igualdad exacta? Las versiones paralelas suman los
 * productos en otro orden (reducciones entre hilos o procesos) y la suma en
 * punto flotante no es asociativa: el resultado cambia en los últimos bits.
 * Un error real de programación (índice mal calculado, condición de carrera,
 * bloque que no llegó) produce diferencias de orden 1, muchísimo mayores.
 */
#ifndef VERIFY_H
#define VERIFY_H

#include "matrix.h"

#define VERIFY_EPS 1e-9

typedef struct {
    int ok;           /* 1 si todos los elementos pasan la tolerancia */
    double max_abs;   /* máximo |x - ref| */
    double max_rel;   /* máximo |x - ref| / |ref| (con |ref| > 0) */
    int bad_i, bad_j; /* primer elemento que falló (-1 si ninguno) */
} VerifyResult;

VerifyResult mat_compare(const Matrix *ref, const Matrix *x, double eps);
int mat_equal_exact(const Matrix *A, const Matrix *B);
void verify_report(const char *label, VerifyResult r);

#endif
