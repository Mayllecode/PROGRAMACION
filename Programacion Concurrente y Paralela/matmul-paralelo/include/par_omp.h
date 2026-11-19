/*
 * par_omp.h — Versiones OpenMP (memoria compartida, semana 7).
 *
 * Idea común a todas: repartir el trabajo de modo que CADA HILO ESCRIBA
 * SOLO SUS PROPIAS FILAS (o bloques) de C. Así no hay dos hilos escribiendo
 * el mismo elemento, no hace falta ningún candado (lock, critical, atomic)
 * y no puede haber condiciones de carrera.
 *
 * Además, cada elemento C[i][j] se suma en el MISMO orden que en la versión
 * secuencial (k creciente), así que el resultado es idéntico bit a bit al
 * de seq.h, con cualquier número de hilos. tests/test_omp.c lo comprueba.
 *
 * El número de hilos se elige fuera: con la variable de entorno
 * OMP_NUM_THREADS o con omp_set_num_threads(p) antes de llamar.
 *
 * Todas devuelven 0 si todo va bien y -1 si las dimensiones no calzan.
 */
#ifndef PAR_OMP_H
#define PAR_OMP_H

#include "matrix.h"
#include "sim.h"

/* Orden i-k-j con las FILAS de C repartidas entre los hilos.
 * La paralelización más simple: una sola línea (#pragma omp parallel for). */
int matmul_omp_ikj(const Matrix *A, const Matrix *B, Matrix *C);

/* Por bloques (tiling) bs x bs: cada hilo toma bloques ENTEROS de C y les
 * acumula A(ii,kk)·B(kk,jj) para todos los kk. El bloque de C queda en la
 * caché del hilo que lo calcula de principio a fin. */
int matmul_omp_blocked(const Matrix *A, const Matrix *B, Matrix *C, int bs);

/* K·U con K en CSR: las filas de la estructura (nodos) se reparten entre
 * los hilos. Misma firma que spmm_csr, así que se enchufa en el simulador:
 * SimConfig.spmm = spmm_csr_omp. */
int spmm_csr_omp(const CSR *A, const Matrix *B, Matrix *C);

/* El simulador completo en paralelo: mismos argumentos y mismo resultado
 * que sim_run, pero además del producto K·U (cfg->spmm) también reparte
 * entre los hilos la actualización de U y el cálculo de los picos.
 * Es necesario por la ley de Amdahl: K·U es solo ~47 % del tiempo del
 * simulador (docs/HITO_H1.md), así que paralelizar solo K·U daría ~1.9×
 * como máximo. */
int sim_run_omp(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
                const Matrix *ag, const Matrix *u0, const Matrix *v0, SimResult *res);

#endif
