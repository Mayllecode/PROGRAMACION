/*
 * mpi_common.c — Piezas comunes de las versiones MPI: mallas de procesos,
 * reparto de bloques de una matriz densa y contadores de comunicación.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mpi_internal.h"

void mpi_stats_reduce(const MpiStats *local, MpiStats *total, MPI_Comm comm) {
    long long in[4] = {local->msgs, local->bytes, local->msgs_dist, local->bytes_dist}, out[4];
    double tin[4] = {local->t_total, local->t_comm, local->t_comp, local->t_dist}, tout[4];
    MPI_Reduce(in, out, 4, MPI_LONG_LONG, MPI_SUM, 0, comm);
    MPI_Reduce(tin, tout, 4, MPI_DOUBLE, MPI_MAX, 0, comm);
    int rank;
    MPI_Comm_rank(comm, &rank);
    if (rank == 0) {
        total->msgs = out[0]; total->bytes = out[1];
        total->msgs_dist = out[2]; total->bytes_dist = out[3];
        total->t_total = tout[0]; total->t_comm = tout[1];
        total->t_comp = tout[2]; total->t_dist = tout[3];
    }
}

int mpi_square_side(int p) {
    int q = (int)(sqrt((double)p) + 0.5);
    return (q >= 1 && q * q == p) ? q : 0;
}

void mpi_grid_dims(int p, int *pr, int *pc) {
    int r = (int)sqrt((double)p);
    while (r > 1 && p % r != 0) r--;
    *pr = r;
    *pc = p / r;
}

int ceil_div(int a, int b) { return (a + b - 1) / b; }

/* La raíz arma un búfer con los bloques de M en el orden de los rangos de
 * `grid` (bloque del proceso r en buf[r * br * bc ...]), rellenando con
 * ceros lo que cae fuera de M, y los reparte con MPI_Scatter.
 * Devuelve el bloque local br x bc (row-major). */
double *scatter_blocks(const Matrix *M, int br, int bc, MPI_Comm grid, MpiStats *st) {
    int rank, p;
    MPI_Comm_rank(grid, &rank);
    MPI_Comm_size(grid, &p);
    const size_t bsz = (size_t)br * bc;
    double *local = xcalloc(bsz, sizeof(double));
    double *buf = NULL;
    if (rank == 0) {
        buf = xcalloc(bsz * p, sizeof(double));
        for (int r = 0; r < p; r++) {
            int c[2];
            MPI_Cart_coords(grid, r, 2, c);
            double *blk = buf + (size_t)r * bsz;
            for (int i = 0; i < br; i++) {
                int gi = c[0] * br + i;
                if (gi >= M->rows) break;
                for (int j = 0; j < bc; j++) {
                    int gj = c[1] * bc + j;
                    if (gj >= M->cols) break;
                    blk[(size_t)i * bc + j] = M_AT(M, gi, gj);
                }
            }
        }
    }
    MPI_Scatter(buf, (int)bsz, MPI_DOUBLE, local, (int)bsz, MPI_DOUBLE, 0, grid);
    if (rank != 0 && bsz) { st->msgs_dist++; st->bytes_dist += (long long)bsz * sizeof(double); }
    free(buf);
    return local;
}

/* Lo inverso: junta los bloques br x bc en la raíz y los copia en M,
 * descartando el relleno. */
void gather_blocks(const double *local, int br, int bc, Matrix *M, MPI_Comm grid, MpiStats *st) {
    int rank, p;
    MPI_Comm_rank(grid, &rank);
    MPI_Comm_size(grid, &p);
    const size_t bsz = (size_t)br * bc;
    double *buf = rank == 0 ? xcalloc(bsz * p, sizeof(double)) : NULL;
    MPI_Gather(local, (int)bsz, MPI_DOUBLE, buf, (int)bsz, MPI_DOUBLE, 0, grid);
    if (rank == 0) {
        st->msgs_dist += p - 1;
        st->bytes_dist += (long long)(p - 1) * bsz * sizeof(double);
        for (int r = 0; r < p; r++) {
            int c[2];
            MPI_Cart_coords(grid, r, 2, c);
            const double *blk = buf + (size_t)r * bsz;
            for (int i = 0; i < br; i++) {
                int gi = c[0] * br + i;
                if (gi >= M->rows) break;
                for (int j = 0; j < bc; j++) {
                    int gj = c[1] * bc + j;
                    if (gj >= M->cols) break;
                    M_AT(M, gi, gj) = blk[(size_t)i * bc + j];
                }
            }
        }
    }
    free(buf);
}

/* C (br x bc) += A (br x bk) · B (bk x bc), orden i-k-j como matmul_ikj.
 * lda/ldb: ancho de fila con que están guardados A y B (para usar un panel
 * dentro de un bloque más ancho sin copiarlo). */
void local_gemm(int br, int bk, int bc, const double *A, int lda, const double *B, int ldb, double *C) {
    for (int i = 0; i < br; i++) {
        double *c = C + (size_t)i * bc;
        for (int k = 0; k < bk; k++) {
            const double a = A[(size_t)i * lda + k];
            const double *b = B + (size_t)k * ldb;
            for (int j = 0; j < bc; j++) c[j] += a * b[j];
        }
    }
}

/* La raíz comprueba las dimensiones de C = A·B y las comunica a todos.
 * dims = {n, l, m}. Devuelve 1 si son válidas. */
int bcast_matmul_dims(const Matrix *A, const Matrix *B, const Matrix *C, MPI_Comm comm, int dims[3]) {
    int rank, v[4] = {0, 0, 0, 0};
    MPI_Comm_rank(comm, &rank);
    if (rank == 0 && A && B && C && A->cols == B->rows && C->rows == A->rows && C->cols == B->cols &&
        A->rows > 0 && A->cols > 0 && B->cols > 0) {
        v[0] = A->rows; v[1] = A->cols; v[2] = B->cols; v[3] = 1;
    }
    MPI_Bcast(v, 4, MPI_INT, 0, comm);
    memcpy(dims, v, 3 * sizeof(int));
    return v[3];
}
