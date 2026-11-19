/*
 * cannon.c — Algoritmo de Cannon (semana 9).
 *
 * p = q² procesos en una malla q x q PERIÓDICA (la fila y la columna se
 * cierran en anillo). El proceso (i, j) guarda el bloque (i, j) de A, de B
 * y de C. Para calcular C(i,j) = Σ_s A(i,s)·B(s,j) necesita, uno a la vez,
 * los q bloques de la fila i de A y de la columna j de B:
 *
 *   Sesgo inicial          A(i, j) <- A(i, j+i)   (la fila i rota i a la izquierda)
 *                          B(i, j) <- B(i+j, j)   (la columna j rota j hacia arriba)
 *   Ahora (i, j) tiene A(i, i+j) y B(i+j, j): los índices internos coinciden.
 *
 *   Repetir q veces:       C(i,j) += A_local · B_local
 *                          A rota 1 a la izquierda, B rota 1 hacia arriba
 *
 * Cada proceso solo habla con sus 4 vecinos y en cada paso todos mandan y
 * reciben a la vez (MPI_Sendrecv_replace): nadie espera a nadie y la memoria
 * por proceso es solo 3 bloques.
 *
 *     Malla 3x3 después del sesgo (A_ij = bloque de A que tiene cada proceso)
 *       (0,0):A00 B00   (0,1):A01 B11   (0,2):A02 B22
 *       (1,0):A11 B10   (1,1):A12 B21   (1,2):A10 B02
 *       (2,0):A22 B20   (2,1):A20 B01   (2,2):A21 B12
 */
#include <stdlib.h>

#include "mpi_internal.h"

static void shift(double *blk, int count, MPI_Comm grid, int dim, int disp, MpiStats *st) {
    if (disp == 0) return;
    int src, dst;
    MPI_Cart_shift(grid, dim, disp, &src, &dst);
    if (src == dst && dst == MPI_PROC_NULL) return;
    double t0 = MPI_Wtime();
    MPI_Sendrecv_replace(blk, count, MPI_DOUBLE, dst, 0, src, 0, grid, MPI_STATUS_IGNORE);
    st->t_comm += MPI_Wtime() - t0;
    st->msgs++;
    st->bytes += (long long)count * sizeof(double);
}

int matmul_cannon(const Matrix *A, const Matrix *B, Matrix *C, MPI_Comm comm, MpiStats *st_out) {
    MpiStats st = {0};
    double t_start = MPI_Wtime();
    int p, dims[3];
    MPI_Comm_size(comm, &p);
    const int q = mpi_square_side(p);
    if (!bcast_matmul_dims(A, B, C, comm, dims) || q == 0) return -1;
    const int n = dims[0], l = dims[1], m = dims[2];

    MPI_Comm grid;
    int gd[2] = {q, q}, per[2] = {1, 1}, me[2], rank;
    MPI_Cart_create(comm, 2, gd, per, 0, &grid);   /* reorder = 0: rango 0 sigue siendo la raíz */
    MPI_Comm_rank(grid, &rank);
    MPI_Cart_coords(grid, rank, 2, me);

    const int br = ceil_div(n, q), bk = ceil_div(l, q), bc = ceil_div(m, q);
    double t0 = MPI_Wtime();
    double *a = scatter_blocks(A, br, bk, grid, &st);
    double *b = scatter_blocks(B, bk, bc, grid, &st);
    double *c = xcalloc((size_t)br * bc, sizeof(double));
    st.t_dist += MPI_Wtime() - t0;

    /* Sesgo inicial: dimensión 1 = columnas de la malla, 0 = filas */
    shift(a, br * bk, grid, 1, -me[0], &st);   /* fila i: i lugares a la izquierda */
    shift(b, bk * bc, grid, 0, -me[1], &st);   /* columna j: j lugares hacia arriba */

    for (int s = 0; s < q; s++) {
        t0 = MPI_Wtime();
        local_gemm(br, bk, bc, a, bk, b, bc, c);
        st.t_comp += MPI_Wtime() - t0;
        if (s + 1 < q) {
            shift(a, br * bk, grid, 1, -1, &st);
            shift(b, bk * bc, grid, 0, -1, &st);
        }
    }

    t0 = MPI_Wtime();
    gather_blocks(c, br, bc, C, grid, &st);
    st.t_dist += MPI_Wtime() - t0;
    free(a); free(b); free(c);
    MPI_Comm_free(&grid);
    st.t_total = MPI_Wtime() - t_start;
    if (st_out) *st_out = st;
    return 0;
}
