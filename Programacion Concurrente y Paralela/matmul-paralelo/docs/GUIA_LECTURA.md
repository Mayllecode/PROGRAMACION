# Guía de lectura de los artículos IEEE (semana 5)

Propuesta de reparto: un integrante por artículo como responsable; cada uno
presenta su ficha al resto en una reunión corta al final de la semana 5. El
artículo ancla (Huang & Chow) lo leen todos al menos en su introducción.

No hace falta entender cada demostración. Lo que buscamos es **qué decisiones
de diseño toman los autores y por qué**, porque son las mismas que vamos a
tener que tomar nosotros.

---

## 1. Artículo ancla — Huang & Chow (IEEE TPDS, 2024)

*Exploring the Design Space of Distributed Parallel Sparse Matrix–Multiple
Vector Multiplication.*

**Por qué lo leemos:** es exactamente nuestro caso disperso: una matriz
dispersa por varios vectores (SpMM), distribuida entre procesos. Nuestra
función `spmm_csr` es la versión secuencial de ese kernel.

**Preguntas para guiar la lectura:**
- ¿Qué formas de repartir la matriz entre procesos comparan (por filas, por
  columnas, en 2D u otras)? ¿Qué gana y qué pierde cada una?
- ¿Cómo cambia el volumen de comunicación según el reparto y según el número
  de vectores?
- ¿Qué matrices usan en sus pruebas y cómo describen su patrón de dispersión?
- ¿Qué métricas reportan y en qué máquinas?

**Alimenta:** semana 11 (reparto por columnas y balanceo por nnz).

## 2. Quintin, Hasanov y Lastovetsky (ICPP 2013) — HSUMMA

*Hierarchical Parallel Matrix Multiplication on Large-Scale Distributed
Memory Platforms.*

**Por qué lo leemos:** parte de SUMMA, que implementamos en la semana 10, y
propone organizar los procesos en una jerarquía de dos niveles para reducir
el costo de comunicación.

**Preguntas para guiar la lectura:**
- ¿Cómo funciona SUMMA básico: qué se difunde (broadcast) en cada paso y a quién?
- ¿Qué cambia en HSUMMA: cómo se agrupan los procesos y en qué niveles se
  hacen las difusiones?
- ¿De qué depende el número de grupos óptimo?
- ¿Bajo qué condiciones la mejora es grande y bajo cuáles es pequeña?
  (La cifra de 5.89× que citamos en la justificación es su mejor caso, con
  16 384 núcleos, y es reducción del tiempo de **comunicación**: el tiempo
  total bajó 2.36×. Con menos de 2048 núcleos los autores midieron casi la
  misma velocidad que SUMMA.)

**Alimenta:** semanas 10 (SUMMA) y 13 (HSUMMA opcional).

## 3. Herault, Robert, Bosilca y Dongarra (ScalA 2019) — PaRSEC

*Generic Matrix Multiplication for Multi-GPU Accelerated Distributed-Memory
Platforms over PaRSEC.*

**Por qué lo leemos:** trata el mismo problema en máquinas con varias GPU por
nodo, que es la dirección de nuestro Nivel 3 (MPI + CUDA).

**Preguntas para guiar la lectura:**
- ¿Cómo dividen el trabajo en tareas y cómo se decide qué tarea va a qué GPU?
- ¿Qué problema aparece por la memoria limitada de la GPU y cómo lo manejan?
- ¿Cómo solapan las transferencias de datos con el cómputo?
- ¿Qué de esto es realista para nosotros (una GPU, pocos procesos) y qué no?

**Alimenta:** semanas 12 y 13 (bloque local en GPU, solapamiento).

---

## Fichas

Los borradores de las tres fichas ya están en `docs/fichas/`, con las cifras
verificadas en cada artículo. Cada responsable debe leer su artículo,
corregirlas y reescribir con sus palabras lo que vaya a exponer.

## Ficha de lectura (copiar una por artículo)

```
Artículo:
Responsable:
Fecha:

1. Problema que resuelven (2-3 líneas, con nuestras palabras):

2. Idea principal de su solución:

3. Decisiones de diseño que nos sirven (y en qué semana las usamos):
   -
   -

4. Cómo miden: máquinas, tamaños, métricas:

5. Resultado principal (con número y en qué condiciones):

6. Qué NO aplica a nuestro caso y por qué:

7. Dudas para discutir en equipo:
```
