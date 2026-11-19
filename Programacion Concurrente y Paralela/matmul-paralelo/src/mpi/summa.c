/*
 * summa.c — SUMMA: Scalable Universal Matrix Multiplication Algorithm (semana 10).
 *
 * Malla pr x pc NO periódica, con cualquier p (6 = 2x3, 8 = 2x4...). El
 * proceso (i, j) guarda el bloque (i, j) de A, de B y de C, como en Cannon.
 *
 * C = A·B = Σ_t A(:, panel t) · B(panel t, :), es decir, una suma de
 * productos "columna por fila" (producto exterior). Para cada panel t:
 *
 *   - el proceso de la fila i que tiene ese panel de A lo DIFUNDE a toda
 *     su fila de procesos (MPI_Bcast en el comunicador de fila),
 *   - el proceso de la columna j que tiene ese panel de B lo difunde a
 *     toda su columna (MPI_Bcast en el comunicador de columna),
 *   - cada proceso hace C_local += A_panel · B_panel.
 *
 * Diferencias con Cannon:
 *   - no hay sesgo inicial ni rotaciones: los bloques no se mueven de dueño;
 *   - funciona con mallas rectangulares (Cannon exige q x q);
 *   - usa difusiones (un emisor, muchos receptores; MPI las hace en árbol)
 *     en vez de mensajes punto a punto entre vecinos.
 *
 * Paneles: el índice interno l se divide en L = mcm(pr, pc) paneles de
 * ancho w, así cada panel cae entero dentro de un bloque de A (que tiene
 * l/pc columnas) y de un bloque de B (que tiene l/pr filas). Con una malla
 * cuadrada q x q, L = q: el mismo número de pasos que Cannon.
 */
#include <stdlib.h>
#include <string.h>

#include "mpi_internal.h"

static int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

int matmul_summa(const Matrix *A, const Matrix *B, Matrix *C, MPI_Comm comm, MpiStats *st_out) {
    MpiStats st = {0};
    double t_start = MPI_Wtime();
    int p, dims[3];
    MPI_Comm_size(comm, &p);
    if (!bcast_matmul_dims(A, B, C, comm, dims)) return -1;
    const int n = dims[0], l = dims[1], m = dims[2];

    int pr, pc;
    mpi_grid_dims(p, &pr, &pc);
    MPI_Comm grid, row_comm, col_comm;
    int gd[2] = {pr, pc}, per[2] = {0, 0}, me[2], rank;
    MPI_Cart_create(comm, 2, gd, per, 0, &grid);
    MPI_Comm_rank(grid, &rank);
    MPI_Cart_coords(grid, rank, 2, me);
    int keep_row[2] = {0, 1}, keep_col[2] = {1, 0};
    MPI_Cart_sub(grid, keep_row, &row_comm);   /* mi fila: rango = mi columna j */
    MPI_Cart_sub(grid, keep_col, &col_comm);   /* mi columna: rango = mi fila i */

    const int L = pr / gcd(pr, pc) * pc;        /* mcm(pr, pc) paneles */
    const int lp = ceil_div(l, L) * L;          /* l rellenado a múltiplo de L */
    const int w = lp / L;                       /* ancho de cada panel */
    const int br = ceil_div(n, pr), bc = ceil_div(m, pc);
    const int ak = lp / pc, bk = lp / pr;       /* columnas del bloque de A, filas del de B */

    double t0 = MPI_Wtime();
    double *a = scatter_blocks(A, br, ak, grid, &st);
    double *b = scatter_blocks(B, bk, bc, grid, &st);
    double *c = xcalloc((size_t)br * bc, sizeof(double));
    double *apan = xcalloc((size_t)br * w, sizeof(double));
    double *bpan = xcalloc((size_t)w * bc, sizeof(double));
    st.t_dist += MPI_Wtime() - t0;

    for (int t = 0; t < L; t++) {
        const int k0 = t * w;
        const int a_owner = k0 / ak, a_off = k0 % ak;   /* columna de la malla que tiene el panel de A */
        const int b_owner = k0 / bk, b_off = k0 % bk;   /* fila de la malla que tiene el panel de B */

        t0 = MPI_Wtime();
        if (me[1] == a_owner)   /* copiar mis w columnas a un panel contiguo */
            for (int i = 0; i < br; i++)
                memcpy(apan + (size_t)i * w, a + (size_t)i * ak + a_off, (size_t)w * sizeof(double));
        MPI_Bcast(apan, br * w, MPI_DOUBLE, a_owner, row_comm);
        if (me[1] != a_owner && pc > 1) { st.msgs++; st.bytes += (long long)br * w * sizeof(double); }

        if (me[0] == b_owner)   /* las w filas de B ya son contiguas */
            memcpy(bpan, b + (size_t)b_off * bc, (size_t)w * bc * sizeof(double));
        MPI_Bcast(bpan, w * bc, MPI_DOUBLE, b_owner, col_comm);
        if (me[0] != b_owner && pr > 1) { st.msgs++; st.bytes += (long long)w * bc * sizeof(double); }
        st.t_comm += MPI_Wtime() - t0;

        t0 = MPI_Wtime();
        local_gemm(br, w, bc, apan, w, bpan, bc, c);
        st.t_comp += MPI_Wtime() - t0;
    }

    t0 = MPI_Wtime();
    gather_blocks(c, br, bc, C, grid, &st);
    st.t_dist += MPI_Wtime() - t0;
    free(a); free(b); free(c); free(apan); free(bpan);
    MPI_Comm_free(&row_comm); MPI_Comm_free(&col_comm); MPI_Comm_free(&grid);
    st.t_total = MPI_Wtime() - t_start;
    if (st_out) *st_out = st;
    return 0;
}
