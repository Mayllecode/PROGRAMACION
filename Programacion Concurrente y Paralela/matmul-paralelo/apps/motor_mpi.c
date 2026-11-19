/*
 * motor_mpi.c — El motor MPI del programa visual (VISUAL.bat).
 *
 * El visual (Go) no puede correr MPI dentro de sí mismo: MPI son VARIOS
 * procesos, cada uno con su memoria. Por eso el visual lanza este programa
 *
 *     mpiexec -n P motor_mpi.exe pisos vanos sismos duracion puerto
 *
 * y le habla por una conexión local (127.0.0.1:puerto) con el proceso 0:
 *
 *   visual -> 'A' n   avanzar n pasos   <- tiempo (s), paso, U completo (n x k doubles)
 *   visual -> 'R' 0   volver al paso 0  <- paso (0)
 *   visual -> 'Q' 0   terminar
 *
 * Al conectarse, el proceso 0 manda: n, k, P, N (pasos) y el reparto
 * (starts[0..P]) y los fantasmas de cada proceso, para dibujar las franjas.
 *
 * El edificio y los sismos son los MISMOS que arma visual/motor_c.c
 * (esc_crear): mismos parámetros, mismo paso de tiempo, mismos 8 sismos.
 * La simulación es simmpi_* de src/mpi/sim_mpi.c, así que U es idéntico bit
 * a bit al del motor Secuencial (el visual lo comprueba en cada cuadro).
 *
 * El tiempo que se devuelve es el del proceso más lento avanzando los n
 * pasos (cálculo + intercambio de fronteras), sin contar mandar U al visual.
 */
#include <math.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_MALO INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_MALO (-1)
#define closesocket close
#endif

#include "building.h"
#include "ground_motion.h"
#include "par_mpi.h"
#include "seq.h"
#include "sim.h"

#define PI 3.14159265358979323846

static int enviar(sock_t s, const void *buf, size_t n) {
    const char *p = buf;
    while (n > 0) {
        int r = send(s, p, (int)(n > (1u << 20) ? (1u << 20) : n), 0);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int recibir(sock_t s, void *buf, size_t n) {
    char *p = buf;
    while (n > 0) {
        int r = recv(s, p, (int)n, 0);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static sock_t conectar(int puerto) {
#ifdef _WIN32
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return SOCK_MALO;
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCK_MALO) return SOCK_MALO;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)puerto);
    a.sin_addr.s_addr = htonl(0x7F000001);   /* 127.0.0.1 */
    if (connect(s, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(s); return SOCK_MALO; }
    int uno = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&uno, sizeof uno);
    return s;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, p;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    if (argc < 6) {
        if (rank == 0) fprintf(stderr, "Uso: mpiexec -n P %s pisos vanos sismos duracion puerto\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    const int pisos = atoi(argv[1]), vanos = atoi(argv[2]), k = atoi(argv[3]), puerto = atoi(argv[5]);
    const double duracion = atof(argv[4]);

    /* --- El escenario, igual que esc_crear (visual/motor_c.c); solo en la raíz --- */
    Building *b = NULL;
    Matrix *ag = NULL;
    SimConfig cfg = {0};
    if (rank == 0) {
        const double m = 2.0e4, kc = 3.5e7, kb = 1.0e8, h = 3.0, zeta = 0.05;
        b = building_create(pisos, vanos, m, kc, kb, h);
        const double T1 = building_fundamental_period(b);
        const double alpha = 2.0 * zeta * (2.0 * PI / T1);
        const double dt = 0.5 * sim_dt_critical(b->K, b->mass);
        const int N = (int)(duracion / dt);
        const double amp = 0.3 * G_ACCEL;
        ag = mat_alloc(N, k);
        double *col = xcalloc((size_t)N, sizeof(double));
        for (int s = 0; s < k; s++) {
            ground_motion_synthetic(col, N, dt, amp, 0.5 + 3.5 * s / (k > 1 ? k - 1 : 1));
            for (int t = 0; t < N; t++) M_AT(ag, t, s) = col[t];
        }
        free(col);
        SimConfig c = {dt, N, k, alpha, spmm_csr, -1};
        cfg = c;
    }
    SimMpi *sm = simmpi_crear(rank == 0 ? b->K : NULL, rank == 0 ? b->mass : NULL, rank == 0 ? b->below : NULL,
                              rank == 0 ? &cfg : NULL, ag, NULL, NULL, MPI_COMM_WORLD, PART_NNZ);
    if (!sm) { MPI_Finalize(); return 1; }
    int fant = simmpi_fantasmas(sm);
    int *fants = rank == 0 ? xcalloc((size_t)p, sizeof(int)) : NULL;
    MPI_Gather(&fant, 1, MPI_INT, fants, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* --- La raíz se conecta con el visual y le cuenta el reparto --- */
    sock_t s = SOCK_MALO;
    int vivo = 1, n = 0;
    Matrix *U = NULL;
    if (rank == 0) {
        n = b->n;
        s = conectar(puerto);
        if (s == SOCK_MALO) vivo = 0;
        else {
            int32_t cab[4] = {n, k, p, cfg.nsteps};
            const int *st = simmpi_starts(sm);
            int32_t *rep = xcalloc((size_t)(2 * p + 1), sizeof(int32_t));
            for (int r = 0; r <= p; r++) rep[r] = st[r];
            for (int r = 0; r < p; r++) rep[p + 1 + r] = fants[r];
            if (enviar(s, cab, sizeof cab) || enviar(s, rep, (size_t)(2 * p + 1) * sizeof(int32_t))) vivo = 0;
            free(rep);
            U = mat_alloc(n, k);
        }
    }

    /* --- Bucle de órdenes: la raíz recibe, todos ejecutan --- */
    while (1) {
        int32_t orden[2] = {'Q', 0};
        if (rank == 0 && vivo) {
            unsigned char c;
            int32_t arg;
            if (recibir(s, &c, 1) || recibir(s, &arg, sizeof arg)) orden[0] = 'Q';
            else { orden[0] = c; orden[1] = arg; }
        }
        MPI_Bcast(orden, 2, MPI_INT32_T, 0, MPI_COMM_WORLD);
        if (orden[0] == 'A') {
            MPI_Barrier(MPI_COMM_WORLD);
            double t0 = MPI_Wtime();
            simmpi_avanzar(sm, orden[1]);
            double t = MPI_Wtime() - t0, tmax = 0.0;
            MPI_Reduce(&t, &tmax, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
            simmpi_juntar(sm, rank == 0 ? U : NULL, NULL, NULL);
            if (rank == 0) {
                int32_t paso = simmpi_paso(sm);
                if (enviar(s, &tmax, sizeof tmax) || enviar(s, &paso, sizeof paso) ||
                    enviar(s, U->data, (size_t)n * k * sizeof(double)))
                    vivo = 0;
            }
        } else if (orden[0] == 'R') {
            simmpi_reiniciar(sm);
            if (rank == 0) {
                int32_t paso = 0;
                if (enviar(s, &paso, sizeof paso)) vivo = 0;
            }
        } else {
            break;
        }
    }

    if (rank == 0) {
        if (s != SOCK_MALO) closesocket(s);
#ifdef _WIN32
        WSACleanup();
#endif
        mat_free(U); mat_free(ag); building_free(b); free(fants);
    }
    simmpi_liberar(sm);
    MPI_Finalize();
    return 0;
}
