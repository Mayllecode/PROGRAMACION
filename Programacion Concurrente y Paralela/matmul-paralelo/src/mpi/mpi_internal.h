/* mpi_internal.h — Funciones auxiliares compartidas por los archivos de src/mpi (no es API pública). */
#ifndef MPI_INTERNAL_H
#define MPI_INTERNAL_H

#include "par_mpi.h"

int ceil_div(int a, int b);
double *scatter_blocks(const Matrix *M, int br, int bc, MPI_Comm grid, MpiStats *st);
void gather_blocks(const double *local, int br, int bc, Matrix *M, MPI_Comm grid, MpiStats *st);
void local_gemm(int br, int bk, int bc, const double *A, int lda, const double *B, int ldb, double *C);
int bcast_matmul_dims(const Matrix *A, const Matrix *B, const Matrix *C, MPI_Comm comm, int dims[3]);

#endif
