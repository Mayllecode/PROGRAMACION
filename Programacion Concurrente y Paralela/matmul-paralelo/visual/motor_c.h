/*
 * motor_c.h — Los motores en C del programa visual (Secuencial y OpenMP).
 *
 * Es el simulador de src/sim.c (secuencial) y src/omp/sim_omp.c (OpenMP),
 * pero avanzando de a POCOS pasos por llamada, para poder dibujar el
 * edificio entre un paso y otro. Las cuentas son las mismas, en el mismo
 * orden, y el producto K·U es el del proyecto: spmm_csr (secuencial) y
 * spmm_csr_omp (OpenMP).
 */
#ifndef MOTOR_C_H
#define MOTOR_C_H

#include "building.h"
#include "matrix.h"

/* Un escenario: el edificio + los k sismos (iguales para los 3 motores) */
typedef struct {
    int pisos, vanos, n, k, nsteps;
    double dt, alpha, T1, h, amp;
    double *freqs;   /* frecuencia de cada sismo [Hz], k valores */
    Building *b;
    Matrix *ag;      /* aceleración del suelo: nsteps x k [m/s^2] */
} Esc;

typedef struct {
    const Esc *e;
    int paralelo;    /* 0 = secuencial (spmm_csr), 1 = OpenMP (spmm_csr_omp) */
    int paso;        /* pasos ya calculados */
    Matrix *Up, *U, *Un, *KU, *peak_u, *peak_drift;
} MotorC;

Esc *esc_crear(int pisos, int vanos, int k, double duracion);
void esc_liberar(Esc *e);

MotorC *mc_crear(const Esc *e, int paralelo);
void mc_reiniciar(MotorC *m);
double mc_avanzar(MotorC *m, int pasos);   /* devuelve los segundos que tardó */
void mc_liberar(MotorC *m);

void omp_hilos(int p);
int nucleos(void);
/* Qué hilo de OpenMP calcula cada fila con schedule(static), el mismo
 * reparto que usa spmm_csr_omp. Lo anota cada hilo, no es una fórmula. */
void duenos_omp(int n, int *out);

#endif
