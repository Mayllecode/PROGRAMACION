/*
 * par_mpi.h — Versiones MPI (memoria distribuida, semanas 9 a 11).
 *
 * En memoria distribuida cada proceso tiene SU PROPIA memoria: nadie puede
 * leer la matriz de otro. Todo dato que un proceso necesita y no tiene hay
 * que mandárselo con un mensaje. Por eso, además del tiempo, aquí se cuentan
 * los MENSAJES y los BYTES (MpiStats).
 *
 * Convención de todas las funciones:
 *   - Son colectivas: TODOS los procesos de `comm` las llaman.
 *   - Los datos de entrada (A, B, K, masas, sismos...) solo tienen que estar
 *     en el proceso 0 ("raíz"); en los demás pueden ser NULL. La raíz los
 *     reparte y al final junta el resultado.
 *   - El resultado (C o SimResult) queda solo en la raíz.
 *   - Devuelven 0 si todo va bien y -1 si los datos no son válidos (el mismo
 *     valor en todos los procesos).
 *
 *   S9   matmul_cannon   Cannon en una malla q x q (p = q² procesos)
 *   S10  matmul_summa    SUMMA en una malla pr x pc (cualquier p)
 *   S10  partition_rows  reparto de los nodos de la estructura
 *   S11  sim_run_mpi     el simulador con la estructura repartida
 *
 * Compilar: con OpenMPI (Linux/WSL) `mpicc`; con MS-MPI (Windows) `gcc` +
 * cabecera y biblioteca de MS-MPI (ver Makefile). El código es el mismo.
 */
#ifndef PAR_MPI_H
#define PAR_MPI_H

#include <mpi.h>

#include "matrix.h"
#include "sim.h"

/* Contadores de comunicación y tiempos de UN proceso. Para el total del
 * programa se suman entre procesos (mpi_stats_reduce). */
typedef struct {
    long long msgs;     /* mensajes RECIBIDOS durante el cálculo */
    long long bytes;    /* bytes RECIBIDOS durante el cálculo */
    long long msgs_dist, bytes_dist;  /* reparto inicial y recolección final (raíz <-> resto) */
    double t_total;     /* segundos de la llamada completa */
    double t_comm;      /* segundos dentro de la comunicación del cálculo */
    double t_comp;      /* segundos calculando (producto local, actualización) */
    double t_dist;      /* segundos repartiendo datos y juntando resultados */
} MpiStats;

/* Suma msgs/bytes de todos los procesos y toma el MÁXIMO de los tiempos
 * (el programa termina cuando termina el más lento). Resultado en la raíz. */
void mpi_stats_reduce(const MpiStats *local, MpiStats *total, MPI_Comm comm);

/* Malla de procesos para Cannon: q = sqrt(p) si p es cuadrado perfecto; 0 si no. */
int mpi_square_side(int p);

/* Malla pr x pc lo más cuadrada posible con pr * pc = p (pr <= pc). Para SUMMA. */
void mpi_grid_dims(int p, int *pr, int *pc);

/* ---------------- S9: Cannon ----------------
 * C = A·B (A: n x l, B: l x m; rectangulares sirven). Exige p = q².
 * Cada proceso (i, j) de la malla guarda un bloque de A, de B y de C.
 *   1. Sesgo inicial: la fila i de bloques de A rota i lugares a la izquierda,
 *      la columna j de B rota j lugares hacia arriba.
 *   2. q veces: C(i,j) += A_local · B_local; A rota 1 a la izquierda y B 1
 *      hacia arriba (MPI_Sendrecv_replace en la malla periódica).
 * Si n, l o m no son múltiplos de q se rellenan con ceros (no cambia C).
 * El orden de las sumas cambia respecto al secuencial: se verifica con ε. */
int matmul_cannon(const Matrix *A, const Matrix *B, Matrix *C, MPI_Comm comm, MpiStats *st);

/* ---------------- S10: SUMMA ----------------
 * Mismo C = A·B en una malla pr x pc con CUALQUIER p (no necesita cuadrado).
 * El índice l se recorre en L = mcm(pr, pc) paneles; en cada uno, el dueño
 * del panel de A lo difunde a su fila de procesos (MPI_Bcast) y el dueño del
 * panel de B a su columna; cada proceso suma A_panel · B_panel a su C. */
int matmul_summa(const Matrix *A, const Matrix *B, Matrix *C, MPI_Comm comm, MpiStats *st);

/* ---------------- S10: reparto de la estructura ----------------
 * Divide las filas 0..rows-1 de K en p rangos CONTIGUOS:
 * el proceso r tiene las filas starts[r] .. starts[r+1]-1 (starts: p + 1).
 *   PART_ROWS: el mismo número de filas (nodos) por proceso.
 *   PART_NNZ:  el mismo número de no nulos por proceso (balanceo de carga:
 *              el costo de K·U es proporcional a nnz, no a las filas). */
enum { PART_ROWS = 0, PART_NNZ = 1 };
void partition_rows(const CSR *K, int p, int mode, int *starts);

/* Desbalance de un reparto: max(nnz por proceso) / promedio. 1.0 = perfecto. */
double partition_imbalance(const CSR *K, int p, const int *starts);

/* ---------------- S11: simulación distribuida ----------------
 * Mismos argumentos y MISMO resultado (bit a bit) que sim_run de sim.h, con
 * los nodos repartidos entre procesos según `part_mode`. En cada paso cada
 * proceso:
 *   1. recibe de sus vecinos los desplazamientos de los nodos de FRONTERA
 *      que necesita (sus "fantasmas") y les manda los suyos,
 *   2. calcula K·U de sus filas (CSR local),
 *   3. avanza U de sus nodos y actualiza sus picos.
 * cfg->spmm se ignora (el producto local es el mismo de spmm_csr).
 * stats (puede ser NULL) recibe las cifras de ESTE proceso. */
typedef struct {
    MpiStats s;
    int part_mode;
    int rows_local;     /* nodos propios */
    int ghosts;         /* nodos fantasma (frontera de los vecinos) */
    int neighbors;      /* procesos con los que intercambia en cada paso */
    long long nnz_local;
} SimMpiStats;

int sim_run_mpi(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
                const Matrix *ag, const Matrix *u0, const Matrix *v0, SimResult *res,
                MPI_Comm comm, int part_mode, SimMpiStats *stats);

/* El mismo simulador, pero por pasos (para el programa visual, que dibuja el
 * edificio entre un paso y otro). Todas son colectivas; los datos entran por
 * la raíz como en sim_run_mpi. simmpi_crear devuelve NULL (en todos) si los
 * datos no son válidos. simmpi_avanzar devuelve los pasos que hizo (se
 * detiene en cfg->nsteps). simmpi_juntar deja u, picos y derivas en la raíz
 * (lo que la raíz pase como NULL no se junta). Mismos números que sim_run. */
typedef struct SimMpi SimMpi;
SimMpi *simmpi_crear(const CSR *K, const double *mass, const int *below, const SimConfig *cfg,
                     const Matrix *ag, const Matrix *u0, const Matrix *v0, MPI_Comm comm, int part_mode);
void simmpi_reiniciar(SimMpi *s);
int simmpi_avanzar(SimMpi *s, int pasos);
void simmpi_juntar(SimMpi *s, Matrix *u, Matrix *peak_u, Matrix *peak_drift);
int simmpi_paso(const SimMpi *s);
const int *simmpi_starts(const SimMpi *s);   /* reparto: filas starts[r] .. starts[r+1]-1 (p + 1 valores) */
int simmpi_fantasmas(const SimMpi *s);       /* nodos fantasma de ESTE proceso */
void simmpi_liberar(SimMpi *s);

/* El producto K·U distribuido, solo (un paso, sin simulador): para probar el
 * reparto y el intercambio de fronteras por separado. Resultado en la raíz. */
int spmm_csr_mpi(const CSR *K, const Matrix *U, Matrix *KU, MPI_Comm comm, int part_mode, MpiStats *st);

#endif
