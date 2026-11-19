/*
 * matrix.h — Estructuras básicas del proyecto.
 *
 * Matrix: matriz densa en orden por filas (row-major).
 *         El elemento (i, j) está en data[i * cols + j].
 * CSR:    matriz dispersa en formato Compressed Sparse Row.
 *         Los valores de la fila i están en val[row_ptr[i] .. row_ptr[i+1]-1]
 *         y sus columnas en col_idx[...] (mismo rango).
 *
 * Todo el proyecto (secuencial, OpenMP, MPI, CUDA) usa estas mismas
 * estructuras, para que la verificación sea siempre la misma.
 */
#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

typedef struct {
    int rows, cols;
    double *data;
} Matrix;

#define M_AT(M, i, j) ((M)->data[(size_t)(i) * (size_t)(M)->cols + (size_t)(j)])

typedef struct {
    int rows, cols, nnz;
    int *row_ptr;   /* tamaño rows + 1 */
    int *col_idx;   /* tamaño nnz */
    double *val;    /* tamaño nnz */
} CSR;

/* calloc que aborta con mensaje si no hay memoria */
void *xcalloc(size_t n, size_t size);

Matrix *mat_alloc(int rows, int cols);           /* inicializada en ceros */
void    mat_free(Matrix *M);
Matrix *mat_copy(const Matrix *A);
void    mat_zero(Matrix *M);
Matrix *mat_transpose(const Matrix *A);
void    mat_print(const Matrix *M, const char *name); /* imprime hasta 8x8 */

CSR    *csr_alloc(int rows, int cols, int nnz);
void    csr_free(CSR *A);
Matrix *csr_to_dense(const CSR *A);
CSR    *dense_to_csr(const Matrix *M);           /* guarda solo valores != 0 */

#endif
