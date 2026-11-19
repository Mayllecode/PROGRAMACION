# Diseño del reparto para MPI (tarea de la semana 8)

Antes de programar MPI (S9 a S11) hay que decidir **qué guarda cada proceso**
y **qué mensajes se mandan**. En memoria compartida (OpenMP) todos los hilos
ven la misma memoria y solo había que repartir el trabajo. En memoria
distribuida cada proceso tiene su propia memoria: lo que no tiene, alguien
se lo tiene que mandar. Un buen reparto mantiene ese tráfico bajo.

Este documento explica las dos decisiones (caso denso y edificio), las
alternativas descartadas y cuánto se comunica. El código está en `src/mpi/`.

---

## 1. Caso denso: bloques en una malla 2D de procesos (Cannon y SUMMA)

Los `p` procesos se ordenan en una malla. El proceso `(i, j)` guarda el
bloque `(i, j)` de A, de B y de C:

```
           columnas de la malla
            j=0     j=1     j=2
   i=0    [A00]   [A01]   [A02]        cada bloque es (n/q) x (n/q)
   i=1    [A10]   [A11]   [A12]        p = 9 procesos, q = 3
   i=2    [A20]   [A21]   [A22]
```

**¿Por qué 2D y no por filas?** Con un reparto por filas, cada proceso
necesita **toda** B: recibe `n²` datos y la comunicación no baja al agregar
procesos. En 2D, cada proceso solo necesita la fila `i` de bloques de A y la
columna `j` de B: recibe `2·n²/√p` datos. Con 16 procesos son 4 veces menos
por proceso que por filas.

| | Cannon (S9) | SUMMA (S10) |
|---|---|---|
| Malla | `q × q`, **periódica** (se cierra en anillo) | `pr × pc`, cualquier `p` |
| Cómo llegan los bloques | Sesgo inicial y luego rotaciones con los 4 vecinos (`MPI_Sendrecv_replace`) | Difusión del panel de A por la fila y del panel de B por la columna (`MPI_Bcast`) |
| Pasos | `q` | `mcm(pr, pc)` (`q` si la malla es cuadrada) |
| Memoria extra | ninguna (rota en el mismo búfer) | dos paneles |
| Restricción | `p` cuadrado perfecto (1, 4, 9, 16) | ninguna |

Si `n` no es múltiplo de `q`, los bloques se **rellenan con ceros**: no
cambia el resultado y el código no necesita casos especiales.

---

## 2. Edificio: franjas de pisos contiguas (simulador, S10–S11)

La matriz K del edificio es dispersa: la fila del nodo `i` tiene no nulos
solo en `i` y sus 4 vecinos (arriba, abajo, izquierda y derecha). Los nodos
se numeran **piso por piso** (`nodo = (piso − 1)·(vanos + 1) + columna`), así
que un rango contiguo de nodos es una **franja de pisos**:

```
   piso 10  o---o---o---o---o  ┐
   piso  9  o---o---o---o---o  ├ proceso 3          o  nodo propio
   piso  8  o---o---o---o---o  ┘                    ◌  nodo fantasma
   - - - - - - - - - - - - - - - frontera 2|3
   piso  7  o---o---o---o---o  ┐  necesita el piso 8 (fantasmas del proceso 3)
   piso  6  o---o---o---o---o  ├ proceso 2
   piso  5  o---o---o---o---o  ┘  necesita el piso 4 (fantasmas del proceso 1)
   ...
```

Para calcular `K·U` de su franja, el proceso 2 necesita el desplazamiento de
una fila de nodos de arriba y una de abajo: sus **nodos fantasma** (copias
de solo lectura de nodos de otro proceso). En cada paso de tiempo los
vecinos se mandan esa fila de frontera.

### 2.1 Cuánto se comunica y cuánto se calcula

Con `W = vanos + 1` nodos por piso, `k` escenarios y `p` procesos:

| | Por proceso y por paso |
|---|---|
| Cálculo de K·U | `≈ 2 · nnz/p · k` operaciones (nnz ≈ 5n) |
| Datos recibidos | `≤ 2 · W · k · 8` bytes (dos filas de frontera) |
| Mensajes | `≤ 2` (vecino de abajo y de arriba) |

El cálculo baja con `p` y la comunicación **no**: siempre son dos filas. La
razón comunicación/cálculo crece como `p / pisos`. Por eso:

- en un edificio alto (100 pisos) con 16 procesos, cada uno tiene ~6 pisos y
  manda 2 de cada ~6: va bien;
- en un edificio de 10 pisos con 16 procesos, a algunos procesos no les
  toca ni un piso completo y la comunicación domina.

Ejemplo, edificio de 100 × 400 (`n = 40 100`, `W = 401`) con `k = 8`:
cada mensaje de frontera es `401 · 8 · 8 ≈ 25.7 KB`; con 16 procesos viajan
30 mensajes por paso (2 por cada una de las 15 fronteras).

### 2.2 Balanceo de carga: por filas o por no nulos

`partition_rows` tiene dos modos:

- **`PART_ROWS`**: el mismo número de nodos por proceso.
- **`PART_NNZ`**: el mismo número de **no nulos** por proceso. El costo de
  `K·U` es proporcional a nnz, no a las filas.

En nuestro edificio casi todas las filas tienen 5 no nulos (los nodos del
primer piso, del techo y de los bordes tienen 3 o 4), así que los dos modos
dan casi lo mismo (desbalance de 1.02 contra 1.01 en un edificio de 30 × 8).
La diferencia aparece con matrices de filas desparejas: en la prueba con
filas de 40 y de 2 no nulos, repartir por filas da un desbalance de **3.5**
(un proceso trabaja 3.5 veces el promedio) y repartir por nnz lo baja a
**1.01**. Lo dejamos implementado porque en S14 se pueden usar edificios
irregulares (pisos con distinto número de vanos).

### 2.3 Orden de un paso de tiempo

```
1. U(n+1) de mis nodos         <- solo datos locales (U, U(n−1), K·U, sismo)
2. rotar U(n−1), U(n), U(n+1)
3. intercambio de fronteras    <- MPI_Irecv / MPI_Isend con cada vecino, MPI_Waitall
4. picos de mis nodos          <- la deriva usa el nodo de abajo (puede ser fantasma)
5. K·U de mis filas            <- usa los fantasmas recién recibidos
```

Un solo intercambio por paso sirve para dos cosas: la deriva (paso 4) y el
producto del paso siguiente (paso 5).

### 2.4 Por qué el resultado es idéntico bit a bit

Cada fila de `K·U` la calcula **un solo** proceso, recorriendo sus no nulos
en el mismo orden que `spmm_csr`. La actualización de U usa la misma
fórmula con las mismas operaciones. Ningún valor se suma en otro orden, así
que no hay ninguna razón para que cambie un bit, con cualquier `p`. Es un
criterio más fuerte que "dentro de ε" y lo comprueba `tests/test_mpi.c`.

En Cannon, en cambio, el bloque `C(i,j)` acumula los productos empezando por
el bloque `(i+j) mod q` del índice interno: el orden de las sumas cambia y
los últimos bits pueden cambiar (se verifica con ε = 1e-9; el error real es
~1e-15). SUMMA recorre los paneles en orden, así que en la práctica coincide
con el secuencial.

---

## 3. Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| Cada paso, juntar U entera en todos (`MPI_Allgather`) | Simple, pero cada proceso recibe `n·k` datos por paso: la comunicación no baja con `p` |
| Reparto 2D del edificio (bloques de pisos × columnas) | Comunica menos cuando hay muchos procesos por piso, pero cada proceso tiene hasta 4 vecinos y la numeración local es más compleja. Con `p ≤ 16` y 100 pisos, cada franja tiene al menos 6 pisos y las franjas alcanzan; queda como mejora |
| Repartir los escenarios (columnas de U) | Sin comunicación, pero solo sirve si `k ≥ p` y cada proceso necesita K entera: no escala con el tamaño del edificio |
| Solapar comunicación y cálculo (calcular el interior mientras llegan las fronteras) | Es la tarea de S13 (cronograma): primero una versión correcta y medida |

---

## 4. Qué se entrega de S8

- [x] Simulación en paralelo de un edificio pequeño con OpenMP (`make demo-omp`, S7)
- [x] Diseño del reparto de la estructura para MPI (este documento)
- [x] Informe de avance: `Exposicion/Informe_avance_S8_S11.pdf` (modelo, verificación y tiempos)
