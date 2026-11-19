/*
 * sim_mpi.c — La estructura repartida entre procesos (semanas 10 y 11).
 *
 * 1. REPARTO (partition_rows). Cada proceso se queda con un rango CONTIGUO
 *    de nodos (filas de K). Como los nodos se numeran piso por piso, un
 *    rango contiguo es una "franja" de pisos del edificio:
 *
 *        piso 10  o---o---o---o---o   proceso 3
 *        piso  9  o---o---o---o---o   proceso 3
 *        piso  8  o---o---o---o---o   proceso 2   <- frontera 2|3: el piso 8
 *        ...                                         necesita el 9 y viceversa
 *
 *    Por número de filas (PART_ROWS) o por número de no nulos (PART_NNZ:
 *    el trabajo de K·U es proporcional a nnz, no al número de nodos).
 *
 * 2. FANTASMAS. La fila i de K tiene no nulos en los vecinos de i (arriba,
 *    abajo, izquierda, derecha). Si un vecino es de otro proceso, su
 *    desplazamiento hay que traerlo en cada paso: es un nodo FANTASMA
 *    (copia local de solo lectura). También el nodo de abajo, para la deriva.
 *    La lista se arma UNA vez al inicio (cada proceso pide a cada vecino los
 *    nodos que necesita; MPI_Alltoall + MPI_Alltoallv).
 *
 * 3. CADA PASO: intercambio de fronteras (MPI_Isend/MPI_Irecv con cada
 *    vecino, y MPI_Waitall) -> K·U local -> avanzar U propio -> picos.
 *
 * Numeración local: los nodos propios van primero (0..nloc-1) y los
 * fantasmas después (nloc..nloc+nghost-1), ordenados por índice global.
 * Como los rangos son contiguos, los fantasmas de un mismo vecino quedan
 * juntos: se reciben directamente en su lugar, sin copiar.
 *
 * Cada valor se calcula con las mismas operaciones y en el mismo orden que
 * sim_run, así que el resultado es IDÉNTICO bit a bit con cualquier p.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mpi_internal.h"

/* ---------------- Reparto ---------------- */

void partition_rows(const CSR *K, int p, int mode, int *starts) {
    const int n = K->rows;
    starts[0] = 0;
    starts[p] = n;
    if (mode == PART_NNZ && K->nnz > 0) {
        /* starts[r] = la fila donde el acumulado de nnz cruza r·nnz/p */
        int i = 0;
        for (int r = 1; r < p; r++) {
            const long long target = (long long)r * K->nnz;   /* comparar row_ptr·p con r·nnz */
            while (i < n && (long long)K->row_ptr[i] * p < target) i++;
            /* quedarse con la frontera más cercana al objetivo (i o i-1) */
            if (i > starts[r - 1] &&
                target - (long long)K->row_ptr[i - 1] * p < (long long)K->row_ptr[i] * p - target)
                i--;
            starts[r] = i < starts[r - 1] ? starts[r - 1] : i;
        }
    } else {
        for (int r = 1; r < p; r++) starts[r] = (int)((long long)r * n / p);
    }
}

double partition_imbalance(const CSR *K, int p, const int *starts) {
    long long mx = 0;
    for (int r = 0; r < p; r++) {
        long long z = K->row_ptr[starts[r + 1]] - K->row_ptr[starts[r]];
        if (z > mx) mx = z;
    }
    return K->nnz > 0 ? (double)mx * p / K->nnz : 1.0;
}

/* ---------------- Distribución de K y plan de intercambio ---------------- */

typedef struct {
    int n, p, rank;
    int *starts;           /* p + 1: reparto (igual en todos) */
    int r0, nloc, nghost;
    CSR *K;                /* filas propias, columnas en numeración local */
    int *ghost;            /* índice global de cada fantasma (ordenado) */
    /* recibir: del vecino recv_rank[t], recv_cnt[t] nodos en ghost[recv_off[t]..] */
    int nrecv, *recv_rank, *recv_off, *recv_cnt;
    /* mandar: al vecino send_rank[t], los nodos locales send_idx[send_off[t]..] */
    int nsend, *send_rank, *send_off, *send_cnt, *send_idx;
    double *sendbuf;
    MPI_Request *req;
    MPI_Status *sts;       /* estados de MPI_Waitall */
} Dist;

static int owner_of(const Dist *d, int g) {   /* búsqueda binaria en starts */
    int lo = 0, hi = d->p - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (d->starts[mid] <= g) lo = mid; else hi = mid - 1;
    }
    while (lo + 1 < d->p && d->starts[lo + 1] <= g) lo++;   /* saltar rangos vacíos */
    return lo;
}

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int local_index(const Dist *d, int g) {
    if (g >= d->r0 && g < d->r0 + d->nloc) return g - d->r0;
    const int *f = bsearch(&g, d->ghost, (size_t)d->nghost, sizeof(int), cmp_int);
    return f ? d->nloc + (int)(f - d->ghost) : -1;
}

/* Reparte las filas de K (desde la raíz) y arma el plan de fantasmas.
 * below_glob: nodo de abajo de cada nodo propio (o NULL): también se
 * agregan como fantasmas. Devuelve el below local en *below_loc. */
static Dist *dist_setup(const CSR *Kroot, int n, MPI_Comm comm, int part_mode,
                        const int *below_root, int has_below, int **below_loc, MpiStats *st) {
    Dist *d = xcalloc(1, sizeof *d);
    MPI_Comm_size(comm, &d->p);
    MPI_Comm_rank(comm, &d->rank);
    const int p = d->p, rank = d->rank;
    d->n = n;
    d->starts = xcalloc((size_t)p + 1, sizeof(int));
    if (rank == 0) partition_rows(Kroot, p, part_mode, d->starts);
    MPI_Bcast(d->starts, p + 1, MPI_INT, 0, comm);
    d->r0 = d->starts[rank];
    d->nloc = d->starts[rank + 1] - d->r0;

    /* --- Filas de K: row_ptr, col_idx y val de mi rango (MPI_Scatterv) --- */
    int *cnt = xcalloc((size_t)p, sizeof(int)), *off = xcalloc((size_t)p, sizeof(int));
    int *zcnt = xcalloc((size_t)p, sizeof(int)), *zoff = xcalloc((size_t)p, sizeof(int));
    if (rank == 0)
        for (int r = 0; r < p; r++) {
            cnt[r] = d->starts[r + 1] - d->starts[r];
            off[r] = d->starts[r];
            zoff[r] = Kroot->row_ptr[d->starts[r]];
            zcnt[r] = Kroot->row_ptr[d->starts[r + 1]] - zoff[r];
        }
    int nnz_loc;
    MPI_Scatter(zcnt, 1, MPI_INT, &nnz_loc, 1, MPI_INT, 0, comm);
    CSR *K = csr_alloc(d->nloc, 1, nnz_loc);   /* cols se fija después */
    int *rp = xcalloc((size_t)d->nloc + 1, sizeof(int));
    /* row_ptr: mando la cantidad de no nulos de cada fila (el row_ptr global
     * menos su comienzo se arma localmente) */
    int *rowlen_root = NULL;
    if (rank == 0) {
        rowlen_root = xcalloc((size_t)n, sizeof(int));
        for (int i = 0; i < n; i++) rowlen_root[i] = Kroot->row_ptr[i + 1] - Kroot->row_ptr[i];
    }
    MPI_Scatterv(rowlen_root, cnt, off, MPI_INT, rp + 1, d->nloc, MPI_INT, 0, comm);
    for (int i = 0; i < d->nloc; i++) rp[i + 1] += rp[i];
    memcpy(K->row_ptr, rp, ((size_t)d->nloc + 1) * sizeof(int));
    MPI_Scatterv(rank == 0 ? Kroot->col_idx : NULL, zcnt, zoff, MPI_INT, K->col_idx, nnz_loc, MPI_INT, 0, comm);
    MPI_Scatterv(rank == 0 ? Kroot->val : NULL, zcnt, zoff, MPI_DOUBLE, K->val, nnz_loc, MPI_DOUBLE, 0, comm);
    int *bl = NULL;
    if (has_below) {
        bl = xcalloc((size_t)d->nloc, sizeof(int));
        MPI_Scatterv(below_root, cnt, off, MPI_INT, bl, d->nloc, MPI_INT, 0, comm);
    }
    if (rank != 0) {
        st->msgs_dist += has_below ? 5 : 4;
        st->bytes_dist += (long long)nnz_loc * (sizeof(int) + sizeof(double)) +
                          (long long)d->nloc * sizeof(int) * (has_below ? 2 : 1) + sizeof(int);
    }
    free(rowlen_root); free(rp);

    /* --- Fantasmas: columnas de K fuera de mi rango, y nodos de abajo --- */
    int cap = nnz_loc + d->nloc + 1, ng = 0;
    int *g = xcalloc((size_t)cap, sizeof(int));
    for (int t = 0; t < nnz_loc; t++) {
        int c = K->col_idx[t];
        if (c < d->r0 || c >= d->r0 + d->nloc) g[ng++] = c;
    }
    if (bl)
        for (int i = 0; i < d->nloc; i++)
            if (bl[i] >= 0 && (bl[i] < d->r0 || bl[i] >= d->r0 + d->nloc)) g[ng++] = bl[i];
    qsort(g, (size_t)ng, sizeof(int), cmp_int);
    int u = 0;
    for (int t = 0; t < ng; t++) if (u == 0 || g[t] != g[u - 1]) g[u++] = g[t];
    d->nghost = u;
    d->ghost = g;
    K->cols = d->nloc + d->nghost;
    for (int t = 0; t < nnz_loc; t++) K->col_idx[t] = local_index(d, K->col_idx[t]);
    if (bl) for (int i = 0; i < d->nloc; i++) if (bl[i] >= 0) bl[i] = local_index(d, bl[i]);
    d->K = K;
    if (below_loc) *below_loc = bl; else free(bl);

    /* --- Plan: a quién le pido qué (fantasmas agrupados por dueño) --- */
    int *need = xcalloc((size_t)p, sizeof(int)), *give = xcalloc((size_t)p, sizeof(int));
    for (int t = 0; t < d->nghost; t++) need[owner_of(d, d->ghost[t])]++;
    MPI_Alltoall(need, 1, MPI_INT, give, 1, MPI_INT, comm);
    d->recv_rank = xcalloc((size_t)p, sizeof(int)); d->recv_off = xcalloc((size_t)p, sizeof(int));
    d->recv_cnt = xcalloc((size_t)p, sizeof(int));
    d->send_rank = xcalloc((size_t)p, sizeof(int)); d->send_off = xcalloc((size_t)p, sizeof(int));
    d->send_cnt = xcalloc((size_t)p, sizeof(int));
    int *noff = xcalloc((size_t)p, sizeof(int)), *goff = xcalloc((size_t)p, sizeof(int));
    int tot_give = 0, acc = 0;
    for (int r = 0; r < p; r++) {
        noff[r] = acc; acc += need[r];
        goff[r] = tot_give; tot_give += give[r];
        if (need[r]) { d->recv_rank[d->nrecv] = r; d->recv_off[d->nrecv] = noff[r]; d->recv_cnt[d->nrecv++] = need[r]; }
        if (give[r]) { d->send_rank[d->nsend] = r; d->send_off[d->nsend] = goff[r]; d->send_cnt[d->nsend++] = give[r]; }
    }
    d->send_idx = xcalloc((size_t)tot_give, sizeof(int));
    MPI_Alltoallv(d->ghost, need, noff, MPI_INT, d->send_idx, give, goff, MPI_INT, comm);
    for (int t = 0; t < tot_give; t++) d->send_idx[t] -= d->r0;   /* global -> local */
    st->msgs_dist += d->nsend;   /* cada pedido recibido */
    st->bytes_dist += (long long)tot_give * sizeof(int);
    d->req = xcalloc((size_t)(d->nrecv + d->nsend), sizeof(MPI_Request));
    d->sts = xcalloc((size_t)(d->nrecv + d->nsend), sizeof(MPI_Status));
    d->sendbuf = NULL;
    free(need); free(give); free(noff); free(goff);
    free(cnt); free(off); free(zcnt); free(zoff);
    return d;
}

static void dist_free(Dist *d) {
    if (!d) return;
    csr_free(d->K);
    free(d->starts); free(d->ghost);
    free(d->recv_rank); free(d->recv_off); free(d->recv_cnt);
    free(d->send_rank); free(d->send_off); free(d->send_cnt); free(d->send_idx);
    free(d->sendbuf); free(d->req); free(d->sts);
    free(d);
}

/* Intercambio de fronteras: las filas fantasma de U (nloc..) se llenan con
 * los valores actuales de sus dueños. U tiene nloc + nghost filas, k columnas. */
static void halo_exchange(Dist *d, Matrix *U, MPI_Comm comm, MpiStats *st) {
    const int k = U->cols;
    double t0 = MPI_Wtime();
    int nr = 0;
    for (int t = 0; t < d->nrecv; t++)
        MPI_Irecv(&M_AT(U, d->nloc + d->recv_off[t], 0), d->recv_cnt[t] * k, MPI_DOUBLE,
                  d->recv_rank[t], 7, comm, &d->req[nr++]);
    if (!d->sendbuf && d->nsend) {
        int tot = d->send_off[d->nsend - 1] + d->send_cnt[d->nsend - 1];
        d->sendbuf = xcalloc((size_t)tot * k, sizeof(double));
    }
    for (int t = 0; t < d->nsend; t++) {
        double *buf = d->sendbuf + (size_t)d->send_off[t] * k;
        for (int x = 0; x < d->send_cnt[t]; x++)
            memcpy(buf + (size_t)x * k, &M_AT(U, d->send_idx[d->send_off[t] + x], 0), (size_t)k * sizeof(double));
        MPI_Isend(buf, d->send_cnt[t] * k, MPI_DOUBLE, d->send_rank[t], 7, comm, &d->req[nr++]);
    }
    MPI_Waitall(nr, d->req, d->sts);
    for (int t = 0; t < d->nrecv; t++) { st->msgs++; st->bytes += (long long)d->recv_cnt[t] * k * sizeof(double); }
    st->t_comm += MPI_Wtime() - t0;
}

/* K·U de las filas propias: el mismo bucle que spmm_csr_rowinit (mismo
 * orden de las sumas que spmm_csr -> mismos bits). */
static void local_spmm(const CSR *K, const Matrix *U, Matrix *KU) {
    const int m = U->cols;
    for (int i = 0; i < K->rows; i++) {
        double *c = &M_AT(KU, i, 0);
        for (int j = 0; j < m; j++) c[j] = 0.0;
        for (int t = K->row_ptr[i]; t < K->row_ptr[i + 1]; t++) {
            const double a = K->val[t];
            const double *b = &M_AT(U, K->col_idx[t], 0);
            for (int j = 0; j < m; j++) c[j] += a * b[j];
        }
    }
}

/* Reparte las filas de una matriz n x k de la raíz (src) a cada proceso
 * (dst: sus nloc primeras filas). */
static void scatter_rows(const Matrix *src, Matrix *dst, const Dist *d, int k, MPI_Comm comm, MpiStats *st) {
    int *cnt = xcalloc((size_t)d->p, sizeof(int)), *off = xcalloc((size_t)d->p, sizeof(int));
    for (int r = 0; r < d->p; r++) { cnt[r] = (d->starts[r + 1] - d->starts[r]) * k; off[r] = d->starts[r] * k; }
    MPI_Scatterv(d->rank == 0 ? src->data : NULL, cnt, off, MPI_DOUBLE, dst->data, d->nloc * k, MPI_DOUBLE, 0, comm);
    if (d->rank != 0) { st->msgs_dist++; st->bytes_dist += (long long)d->nloc * k * sizeof(double); }
    free(cnt); free(off);
}

/* Junta en la raíz (dst, n x k) las nloc primeras filas de cada proceso. */
static void gather_rows(const Matrix *src, Matrix *dst, const Dist *d, int k, MPI_Comm comm, MpiStats *st) {
    int *cnt = xcalloc((size_t)d->p, sizeof(int)), *off = xcalloc((size_t)d->p, sizeof(int));
    for (int r = 0; r < d->p; r++) { cnt[r] = (d->starts[r + 1] - d->starts[r]) * k; off[r] = d->starts[r] * k; }
    MPI_Gatherv(src->data, d->nloc * k, MPI_DOUBLE, d->rank == 0 ? dst->data : NULL, cnt, off, MPI_DOUBLE, 0, comm);
    if (d->rank == 0) { st->msgs_dist += d->p - 1; st->bytes_dist += (long long)(d->n - d->nloc) * k * sizeof(double); }
    free(cnt); free(off);
}

/* ---------------- K·U distribuido (un solo producto) ---------------- */

int spmm_csr_mpi(const CSR *K, const Matrix *U, Matrix *KU, MPI_Comm comm, int part_mode, MpiStats *st_out) {
    MpiStats st = {0};
    double t_start = MPI_Wtime();
    int rank, v[3] = {0, 0, 0};
    MPI_Comm_rank(comm, &rank);
    if (rank == 0 && K && U && KU && K->cols == U->rows && KU->rows == K->rows && KU->cols == U->cols &&
        K->rows == K->cols && U->cols > 0) {
        v[0] = K->rows; v[1] = U->cols; v[2] = 1;
    }
    MPI_Bcast(v, 3, MPI_INT, 0, comm);
    if (!v[2]) return -1;
    const int n = v[0], k = v[1];

    double t0 = MPI_Wtime();
    Dist *d = dist_setup(K, n, comm, part_mode, NULL, 0, NULL, &st);
    Matrix *Ul = mat_alloc(d->nloc + d->nghost, k), *KUl = mat_alloc(d->nloc, k);
    scatter_rows(U, Ul, d, k, comm, &st);
    st.t_dist += MPI_Wtime() - t0;

    halo_exchange(d, Ul, comm, &st);
    t0 = MPI_Wtime();
    local_spmm(d->K, Ul, KUl);
    st.t_comp += MPI_Wtime() - t0;

    t0 = MPI_Wtime();
    gather_rows(KUl, KU, d, k, comm, &st);
    st.t_dist += MPI_Wtime() - t0;
    mat_free(Ul); mat_free(KUl);
    dist_free(d);
    st.t_total = MPI_Wtime() - t_start;
    if (st_out) *st_out = st;
    return 0;
}

/* ---------------- S11: el simulador distribuido ---------------- */

static void peaks_rows(const Matrix *U, int nloc, const int *below, Matrix *pu, Matrix *pd) {
    const int k = U->cols;
    for (int i = 0; i < nloc; i++)
        for (int s = 0; s < k; s++) {
            double u = M_AT(U, i, s);
            if (fabs(u) > M_AT(pu, i, s)) M_AT(pu, i, s) = fabs(u);
            if (below) {
                double dd = below[i] >= 0 ? u - M_AT(U, below[i], s) : u;
                if (fabs(dd) > M_AT(pd, i, s)) M_AT(pd, i, s) = fabs(dd);
            }
        }
}

/* ---------------- El simulador por pasos (SimMpi) ----------------
 * sim_run_mpi corre todos los pasos de una vez. El programa visual necesita
 * avanzar de a pocos pasos para dibujar el edificio entre uno y otro, así
 * que el simulador se guarda en un estado (SimMpi) que se crea, se avanza y
 * se junta. sim_run_mpi (más abajo) es crear + avanzar N pasos + juntar:
 * las cuentas son exactamente las mismas, en el mismo orden. */

struct SimMpi {
    MPI_Comm comm;
    int rank, p, n, k, N, rec, part_mode, paso;
    double dt, a, inv_dt2, c1, c2;
    Dist *d;
    int *below;
    double *mass;
    Matrix *ag, *U0, *V0;           /* sismo (entero) y condiciones iniciales (filas propias) */
    Matrix *Up, *U, *Un, *KU, *pu, *pd;
    int rec_owner, rec_loc;
    double *hist;
    double t_kernel;                /* segundos en K·U (este proceso) */
    MpiStats st;
};

/* Paso 0: U = U0, U(-1) por la fórmula de arranque, picos de U(0) */
static void simmpi_arranque(SimMpi *s) {
    Dist *d = s->d;
    const int nloc = d->nloc, k = s->k;
    mat_zero(s->Up); mat_zero(s->U); mat_zero(s->Un); mat_zero(s->pu); mat_zero(s->pd);
    if (s->U0) memcpy(s->U->data, s->U0->data, (size_t)nloc * k * sizeof(double));
    halo_exchange(d, s->U, s->comm, &s->st);
    double t0 = MPI_Wtime();
    local_spmm(d->K, s->U, s->KU);
    s->t_kernel += MPI_Wtime() - t0;
    for (int i = 0; i < nloc; i++)
        for (int j = 0; j < k; j++) {
            double v = s->V0 ? M_AT(s->V0, i, j) : 0.0;
            double g = s->ag ? M_AT(s->ag, 0, j) : 0.0;
            double acc = -g - M_AT(s->KU, i, j) / s->mass[i] - s->a * v;
            M_AT(s->Up, i, j) = M_AT(s->U, i, j) - s->dt * v + 0.5 * s->dt * s->dt * acc;
        }
    peaks_rows(s->U, nloc, s->below, s->pu, s->pd);
    if (s->hist) for (int j = 0; j < k; j++) s->hist[j] = M_AT(s->U, s->rec_loc, j);
    s->st.t_comp += MPI_Wtime() - t0;
    s->paso = 0;
}

SimMpi *simmpi_crear(const CSR *Kroot, const double *mass_root, const int *below_root, const SimConfig *cfg,
                     const Matrix *ag_root, const Matrix *u0, const Matrix *v0, MPI_Comm comm, int part_mode) {
    int rank, p;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &p);
    /* --- La raíz valida (las mismas reglas que sim_run) y difunde los escalares --- */
    int iv[8] = {0};    /* n, k, N, record_node, has_ag, has_u0, has_v0, has_below */
    double dv[2] = {0.0, 0.0};
    int ok = 0;
    if (rank == 0 && Kroot && mass_root && cfg) {
        const int n = Kroot->rows, k = cfg->k, N = cfg->nsteps;
        ok = Kroot->rows == Kroot->cols && k >= 1 && N >= 1 && cfg->dt > 0.0 &&
             !(ag_root && (ag_root->rows < N || ag_root->cols != k)) &&
             !(u0 && (u0->rows != n || u0->cols != k)) &&
             !(v0 && (v0->rows != n || v0->cols != k)) && cfg->record_node < n;
        iv[0] = n; iv[1] = k; iv[2] = N; iv[3] = cfg->record_node;
        iv[4] = ag_root != NULL; iv[5] = u0 != NULL; iv[6] = v0 != NULL; iv[7] = below_root != NULL;
        dv[0] = cfg->dt; dv[1] = cfg->alpha;
    }
    MPI_Bcast(&ok, 1, MPI_INT, 0, comm);
    if (!ok) return NULL;
    MPI_Bcast(iv, 8, MPI_INT, 0, comm);
    MPI_Bcast(dv, 2, MPI_DOUBLE, 0, comm);

    SimMpi *s = xcalloc(1, sizeof *s);
    s->comm = comm; s->rank = rank; s->p = p; s->part_mode = part_mode;
    s->n = iv[0]; s->k = iv[1]; s->N = iv[2]; s->rec = iv[3];
    s->dt = dv[0]; s->a = dv[1];
    s->inv_dt2 = 1.0 / (s->dt * s->dt);
    s->c1 = s->inv_dt2 + s->a / (2.0 * s->dt);
    s->c2 = s->inv_dt2 - s->a / (2.0 * s->dt);
    const int k = s->k, N = s->N;

    /* --- Reparto: K, masas, nodo de abajo, sismos, condiciones iniciales --- */
    double t0 = MPI_Wtime();
    s->d = dist_setup(Kroot, s->n, comm, part_mode, below_root, iv[7], &s->below, &s->st);
    Dist *d = s->d;
    const int nloc = d->nloc, nall = d->nloc + d->nghost;
    s->mass = xcalloc((size_t)nloc, sizeof(double));
    {
        int *cnt = xcalloc((size_t)p, sizeof(int)), *off = xcalloc((size_t)p, sizeof(int));
        for (int r = 0; r < p; r++) { cnt[r] = d->starts[r + 1] - d->starts[r]; off[r] = d->starts[r]; }
        MPI_Scatterv(mass_root, cnt, off, MPI_DOUBLE, s->mass, nloc, MPI_DOUBLE, 0, comm);
        if (rank != 0) { s->st.msgs_dist++; s->st.bytes_dist += (long long)nloc * sizeof(double); }
        free(cnt); free(off);
    }
    if (iv[4]) {   /* el sismo es el mismo para todos los nodos: cada proceso lo necesita entero */
        s->ag = mat_alloc(N, k);
        if (rank == 0) memcpy(s->ag->data, ag_root->data, (size_t)N * k * sizeof(double));
        MPI_Bcast(s->ag->data, N * k, MPI_DOUBLE, 0, comm);
        if (rank != 0) { s->st.msgs_dist++; s->st.bytes_dist += (long long)N * k * sizeof(double); }
    }
    s->Up = mat_alloc(nall, k); s->U = mat_alloc(nall, k); s->Un = mat_alloc(nall, k);
    s->KU = mat_alloc(nloc ? nloc : 1, k);
    if (iv[5]) { s->U0 = mat_alloc(nloc ? nloc : 1, k); scatter_rows(u0, s->U0, d, k, comm, &s->st); }
    if (iv[6]) { s->V0 = mat_alloc(nloc ? nloc : 1, k); scatter_rows(v0, s->V0, d, k, comm, &s->st); }
    s->pu = mat_alloc(nloc ? nloc : 1, k); s->pd = mat_alloc(nloc ? nloc : 1, k);
    s->rec_owner = s->rec >= 0 ? owner_of(d, s->rec) : -1;
    s->rec_loc = (s->rec_owner == rank) ? s->rec - d->r0 : -1;
    if (s->rec_owner == rank) s->hist = xcalloc(((size_t)N + 1) * (size_t)k, sizeof(double));
    s->st.t_dist += MPI_Wtime() - t0;

    simmpi_arranque(s);
    return s;
}

void simmpi_reiniciar(SimMpi *s) {
    simmpi_arranque(s);
}

int simmpi_avanzar(SimMpi *s, int pasos) {
    Dist *d = s->d;
    const int nloc = d->nloc, k = s->k, N = s->N;
    int hechos = 0;
    while (hechos < pasos && s->paso < N) {
        const int step = s->paso;
        double t0 = MPI_Wtime();
        for (int i = 0; i < nloc; i++) {
            const double inv_m = 1.0 / s->mass[i];
            for (int j = 0; j < k; j++) {
                double g = s->ag ? M_AT(s->ag, step, j) : 0.0;
                double rhs = 2.0 * s->inv_dt2 * M_AT(s->U, i, j) - s->c2 * M_AT(s->Up, i, j)
                             - g - M_AT(s->KU, i, j) * inv_m;
                M_AT(s->Un, i, j) = rhs / s->c1;
            }
        }
        Matrix *tmp = s->Up; s->Up = s->U; s->U = s->Un; s->Un = tmp;
        s->st.t_comp += MPI_Wtime() - t0;

        halo_exchange(d, s->U, s->comm, &s->st);   /* fronteras de U(n+1): para la deriva y para K·U */

        t0 = MPI_Wtime();
        peaks_rows(s->U, nloc, s->below, s->pu, s->pd);
        if (s->hist) for (int j = 0; j < k; j++) s->hist[(size_t)(step + 1) * k + j] = M_AT(s->U, s->rec_loc, j);
        if (step + 1 < N) {
            double tk = MPI_Wtime();
            local_spmm(d->K, s->U, s->KU);
            s->t_kernel += MPI_Wtime() - tk;
        }
        s->st.t_comp += MPI_Wtime() - t0;
        s->paso++;
        hechos++;
    }
    return hechos;
}

int simmpi_paso(const SimMpi *s) { return s->paso; }
const int *simmpi_starts(const SimMpi *s) { return s->d->starts; }
int simmpi_fantasmas(const SimMpi *s) { return s->d->nghost; }

void simmpi_juntar(SimMpi *s, Matrix *u, Matrix *pu, Matrix *pd) {
    double t0 = MPI_Wtime();
    int want[3] = {u != NULL, pu != NULL, pd != NULL};   /* lo que pidió la raíz */
    MPI_Bcast(want, 3, MPI_INT, 0, s->comm);
    if (want[0]) gather_rows(s->U, s->rank == 0 ? u : NULL, s->d, s->k, s->comm, &s->st);
    if (want[1]) gather_rows(s->pu, s->rank == 0 ? pu : NULL, s->d, s->k, s->comm, &s->st);
    if (want[2]) gather_rows(s->pd, s->rank == 0 ? pd : NULL, s->d, s->k, s->comm, &s->st);
    s->st.t_dist += MPI_Wtime() - t0;
}

void simmpi_liberar(SimMpi *s) {
    if (!s) return;
    free(s->hist); free(s->mass); free(s->below);
    mat_free(s->ag); mat_free(s->U0); mat_free(s->V0);
    mat_free(s->Up); mat_free(s->U); mat_free(s->Un); mat_free(s->KU);
    mat_free(s->pu); mat_free(s->pd);
    dist_free(s->d);
    free(s);
}

int sim_run_mpi(const CSR *Kroot, const double *mass_root, const int *below_root, const SimConfig *cfg,
                const Matrix *ag_root, const Matrix *u0, const Matrix *v0, SimResult *res,
                MPI_Comm comm, int part_mode, SimMpiStats *stats) {
    double t_start = MPI_Wtime();
    int rank;
    MPI_Comm_rank(comm, &rank);
    if (rank == 0 && !res) cfg = NULL;   /* sin dónde dejar el resultado: datos inválidos */
    SimMpi *s = simmpi_crear(Kroot, mass_root, below_root, cfg, ag_root, u0, v0, comm, part_mode);
    if (!s) return -1;
    simmpi_avanzar(s, s->N);

    /* --- Juntar resultados en la raíz --- */
    const int n = s->n, k = s->k, N = s->N;
    Matrix *ru = NULL, *rpu = NULL, *rpd = NULL;
    if (rank == 0) {
        memset(res, 0, sizeof *res);
        res->u = ru = mat_alloc(n, k);
        res->peak_u = rpu = mat_alloc(n, k);
        res->peak_drift = rpd = mat_alloc(n, k);
    }
    simmpi_juntar(s, ru, rpu, rpd);
    double t0 = MPI_Wtime();
    if (s->rec >= 0) {
        const int hn = (N + 1) * k;
        if (rank == 0) {
            res->history = s->hist ? s->hist : xcalloc((size_t)hn, sizeof(double));
            if (!s->hist) {
                MPI_Recv(res->history, hn, MPI_DOUBLE, s->rec_owner, 9, comm, MPI_STATUS_IGNORE);
                s->st.msgs_dist++; s->st.bytes_dist += (long long)hn * sizeof(double);
            }
            s->hist = NULL;
        } else if (s->hist) {
            MPI_Send(s->hist, hn, MPI_DOUBLE, 0, 9, comm);
        }
    }
    s->st.t_dist += MPI_Wtime() - t0;
    s->st.t_total = MPI_Wtime() - t_start;

    double tin[2] = {s->t_kernel, s->st.t_total}, tout[2];
    MPI_Reduce(tin, tout, 2, MPI_DOUBLE, MPI_MAX, 0, comm);
    if (rank == 0) { res->t_kernel = tout[0]; res->t_total = tout[1]; }
    if (stats) {
        stats->s = s->st;
        stats->part_mode = part_mode;
        stats->rows_local = s->d->nloc;
        stats->ghosts = s->d->nghost;
        stats->neighbors = s->d->nrecv;
        stats->nnz_local = s->d->K->nnz;
    }
    simmpi_liberar(s);
    return 0;
}
