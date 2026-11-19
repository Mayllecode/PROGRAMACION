/*
 * seq.h — Versiones secuenciales de C = A · B.
 *
 * Son la REFERENCIA del proyecto: toda versión paralela (OpenMP, Go, MPI,
 * CUDA) debe dar el mismo resultado que matmul_naive dentro de la
 * tolerancia definida en verify.h.
 *
 * Todas devuelven 0 si todo va bien y -1 si las dimensiones no calzan.
 * C debe venir reservada con tamaño A->rows x B->cols.
 */
#ifndef SEQ_H
#define SEQ_H

#include "matrix.h"

/* Tres bucles clásicos i-j-k. O(n^3). La referencia "que nadie discute". */
int matmul_naive(const Matrix *A, const Matrix *B, Matrix *C);

/* Orden i-k-j: recorre B y C por filas (acceso contiguo en memoria).
 * Mismo número de operaciones, mucho mejor uso de caché. */
int matmul_ikj(const Matrix *A, const Matrix *B, Matrix *C);

/* Por bloques (tiling) de tamaño bs x bs: cada bloque entra en caché.
 * Es la misma descomposición que después se reparte entre hilos/procesos. */
int matmul_blocked(const Matrix *A, const Matrix *B, Matrix *C, int bs);

/* C = A · B con A dispersa (CSR) y B densa de pocas columnas
 * (SpMM: "sparse matrix – multiple vectors", el kernel de Huang & Chow). */
int spmm_csr(const CSR *A, const Matrix *B, Matrix *C);

/* Igual que spmm_csr, pero cada fila de C se pone en cero justo antes de
 * acumularla (cuando ya está en caché) en vez de limpiar toda C antes con
 * mat_zero. Mismo resultado bit a bit y una pasada menos por memoria.
 * Agregada en S7: la versión OpenMP lo hace así, y sin esta variante el
 * speedup saldría inflado (S(1) ≈ 1.3 con un solo hilo, ver docs/GUIA_S7.md).
 * Es la mejor versión secuencial del caso disperso: la T(1) de K·U. */
int spmm_csr_rowinit(const CSR *A, const Matrix *B, Matrix *C);

#endif
