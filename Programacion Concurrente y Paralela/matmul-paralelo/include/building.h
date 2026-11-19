/*
 * building.h — Modelo matricial simplificado de un edificio (pórtico 2D).
 *
 * El edificio tiene `pisos` niveles y `vanos` vanos, es decir (vanos + 1)
 * líneas de columnas. Hay un nodo en cada cruce piso–columna, por encima del
 * suelo. Cada nodo tiene UN grado de libertad: su desplazamiento horizontal
 * relativo al suelo.
 *
 *     piso 3   o----o----o----o        o  nodo (masa m)
 *              |    |    |    |        |  columna: resorte kc entre pisos
 *     piso 2   o----o----o----o        -  losa/viga: resorte kb entre
 *              |    |    |    |           nodos vecinos del mismo piso
 *     piso 1   o----o----o----o
 *              |    |    |    |
 *     suelo  ////////////////////      (base empotrada: desplazamiento 0)
 *
 * Supuestos: lineal, elástico, 2D, masas concentradas en los nodos.
 * Es un modelo académico para estudiar el cómputo, NO un modelo de diseño.
 *
 * Numeración: nodo (piso p = 1..pisos, columna c = 0..vanos)
 *             -> índice (p - 1) * (vanos + 1) + c
 */
#ifndef BUILDING_H
#define BUILDING_H

#include "matrix.h"

typedef struct {
    int pisos, vanos;
    int n;          /* grados de libertad = pisos * (vanos + 1) */
    double h;       /* altura de entrepiso [m] */
    CSR *K;         /* matriz de rigidez lateral [N/m], n x n, simétrica */
    double *mass;   /* masa de cada nodo [kg] */
    int *below;     /* índice del nodo de abajo; -1 si debajo está el suelo */
} Building;

/* m: masa por nodo [kg]; kc: rigidez lateral de una columna en un
 * entrepiso [N/m]; kb: rigidez de la losa entre nodos vecinos [N/m];
 * h: altura de entrepiso [m]. */
Building *building_create(int pisos, int vanos, double m, double kc, double kb, double h);
void building_free(Building *b);

/* Periodo fundamental T1 [s] por el cociente de Rayleigh,
 *     ω1² = (φᵀ·K·φ) / (φᵀ·M·φ),   φ(piso p) = sin(π·p / (2·pisos + 1)),
 * que es la forma del primer modo de un edificio de cortante uniforme.
 * En este modelo todos los pisos son iguales, así que φ es el modo exacto y
 * T1 = 2π/ω1 coincide con la fórmula cerrada (se verifica en test_sim). */
double building_fundamental_period(const Building *b);

static inline int building_node(const Building *b, int piso, int col) {
    return (piso - 1) * (b->vanos + 1) + col;
}

#endif
