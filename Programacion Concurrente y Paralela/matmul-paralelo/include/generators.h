/*
 * generators.h — Matrices de prueba reproducibles.
 *
 * Todas las matrices aleatorias usan un generador propio (splitmix64) con
 * semilla explícita: la misma semilla produce la misma matriz en cualquier
 * máquina y sistema operativo. Así cada integrante puede reproducir
 * exactamente el caso de prueba de otro.
 */
#ifndef GENERATORS_H
#define GENERATORS_H

#include "matrix.h"

/* Identidad n x n. A·I = I·A = A (resultado conocido de antemano). */
Matrix *gen_identity(int n);

/* Permutación n x n: P[i][perm[i]] = 1.
 * (P·A) tiene en la fila i la fila perm[i] de A. Si perm_out != NULL
 * se copia ahí la permutación usada (arreglo de n enteros). */
Matrix *gen_permutation(int n, unsigned long long seed, int *perm_out);

/* Densa aleatoria rows x cols con valores uniformes en [-1, 1). */
Matrix *gen_random(int rows, int cols, unsigned long long seed);

/* Matriz de rigidez de una malla 2D de nx x ny nodos (esténcil de 5 puntos):
 * cada nodo se conecta con sus vecinos arriba, abajo, izquierda y derecha.
 * Diagonal = 4, vecinos = -1. Es simétrica, dispersa y tiene el mismo
 * patrón que las matrices de rigidez del Método de Elementos Finitos:
 * cada nodo solo interactúa con sus vecinos inmediatos.
 * Tamaño: (nx*ny) x (nx*ny). */
CSR *gen_stiffness_2d(int nx, int ny);

/* Dispersa aleatoria: cada posición es no nula con probabilidad `density`.
 * Los valores no nulos tienen magnitud en [1, 2) y signo aleatorio. */
CSR *gen_random_sparse(int rows, int cols, double density, unsigned long long seed);

#endif
