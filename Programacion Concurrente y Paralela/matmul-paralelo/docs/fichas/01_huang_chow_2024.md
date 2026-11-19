# Ficha de lectura 1 — Huang & Chow (IEEE TPDS, 2024)

```
Artículo:    H. Huang y E. Chow, "Exploring the Design Space of Distributed Parallel
             Sparse Matrix-Multiple Vector Multiplication", IEEE Transactions on
             Parallel and Distributed Systems, vol. 35, n.º 11, pp. 1977-1988, 2024.
             DOI: 10.1109/TPDS.2024.3452478   (Georgia Institute of Technology)
Responsable: B
Fecha:       2026-10-01 (borrador)
```

> **Estado:** borrador hecho a partir del **resumen publicado** (IEEE Xplore,
> ACM DL y la página del autor). El texto completo está detrás del muro de
> pago de IEEE: el responsable debe descargarlo con la cuenta de la biblioteca
> UNMSM y completar las secciones 4 y 5 **con datos del PDF**, no de memoria.
> Lo que está aquí es solo lo que dice el resumen o lo que razonamos nosotros
> (marcado como *nuestro*).

**1. Problema que resuelven.**
Multiplicar una matriz dispersa por un bloque de varios vectores densos (SpMM)
repartiendo el trabajo entre procesos de memoria distribuida. Con un solo
vector (SpMV) la forma de repartir está muy estudiada; con varios vectores
aparecen más formas de repartir, y la mejor no es obvia.

**2. Idea principal.**
Según el resumen: un reparto **más grueso de la matriz dispersa combinado con
un reparto por columnas del bloque de vectores** suele necesitar **menos volumen
de comunicación** y da mejor rendimiento que repartir solo la matriz. Proponen
un algoritmo que **elige la geometría de la malla de procesos** para un número
dado de procesos, y que puede complementar a los particionadores de grafos
existentes aprovechando la concurrencia extra que dan los vectores múltiples.

**3. Decisiones de diseño que nos sirven.**
- *Nuestro:* es exactamente nuestro kernel. En el simulador, K es la matriz
  dispersa y U (n × k) es el bloque de vectores: **una columna por escenario
  sísmico**. `spmm_csr` en `src/seq.c` es la versión secuencial.
- *Nuestro:* hay dos ejes para repartir. (a) Filas de K = nodos del edificio
  → cada proceso necesita los valores de U de sus nodos de frontera en cada
  paso (intercambio de "halo"). (b) Columnas de U = escenarios → los
  escenarios son independientes, no hay comunicación en K·U, pero cada proceso
  necesita K completa. El artículo estudia justamente la combinación de los dos
  (malla de procesos filas × columnas). → **S11** (reparto y balanceo por nnz).
- La geometría de la malla de procesos se elige, no se fija: lo mismo haremos
  al comparar 1D (solo filas) contra 2D en S11.

**4. Cómo miden (completar con el PDF).** Máquinas, matrices de prueba (y su
patrón de dispersión), número de vectores, métricas.

**5. Resultado principal (completar con el PDF).** Número y condiciones.

**6. Qué NO aplica a nuestro caso.** *Nuestro:* nuestra K tiene un patrón muy
regular (máximo 5 no nulos por fila, pórtico 2D): un reparto por bloques de
pisos ya es casi óptimo y no necesitamos un particionador de grafos. Además
tendremos pocos procesos (4 a 16), no cientos.

**7. Dudas para discutir.**
- ¿Con cuántos escenarios k conviene repartir también las columnas de U?
- ¿El volumen de comunicación que modelan incluye el reparto inicial de K?

Fuentes del borrador: [IEEE Xplore](https://ieeexplore.ieee.org/document/10660551/) ·
[ACM DL](https://dl.acm.org/doi/10.1109/TPDS.2024.3452478) ·
[publicaciones de E. Chow](https://faculty.cc.gatech.edu/~echow/publications.html)
