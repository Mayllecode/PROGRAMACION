# Ficha de lectura 3 — Herault, Robert, Bosilca y Dongarra (ScalA 2019): GEMM sobre PaRSEC

```
Artículo:    T. Herault, Y. Robert, G. Bosilca y J. Dongarra, "Generic Matrix
             Multiplication for Multi-GPU Accelerated Distributed-Memory Platforms
             over PaRSEC", 10th IEEE/ACM Workshop on Latest Advances in Scalable
             Algorithms for Large-Scale Systems (ScalA 2019), pp. 33-41.
             DOI: 10.1109/ScalA49573.2019.00010   (ICL, University of Tennessee)
Responsable: D
Fecha:       2026-10-01 (borrador)
```

> **Estado:** borrador hecho con el **texto completo** (PDF público en el sitio
> del ICL). Las cifras están copiadas del artículo; el responsable debe
> revisarlo y reescribir con sus palabras lo que vaya a exponer.

**1. Problema que resuelven.**
Multiplicar C = A × B en máquinas de memoria distribuida cuyos nodos tienen
**varias GPU** (por ejemplo Summit: 6 GPU por nodo). La única biblioteca
pública para eso (SLATE) exigía que **C entera cupiera en la memoria de las
GPU**. Ellos quieren matrices de cualquier tamaño.

**2. Idea principal.**
El algoritmo clásico de producto exterior por *tiles* (la familia de SUMMA),
más **dependencias de control** que limitan qué datos están vivos a la vez:
- C se divide en **bloques** (b × c tiles por proceso) que se calculan uno
  tras otro: solo viajan por la red las filas de A y columnas de B que ese
  bloque necesita (si no, la red se inunda).
- Dentro de cada bloque, la suma en k se parte en **chunks** de d tiles, para
  que lo necesario quepa en la memoria de cada GPU.
- **Lookahead ℓ**: mientras se calcula un chunk se **precargan** los datos de
  los ℓ + 1 siguientes (solapar transferencia y cómputo).
- Las columnas de C se reparten entre las GPU del nodo en forma cíclica.
- Está escrito sobre el runtime de tareas **PaRSEC** (grafo de tareas PTG).

**3. Decisiones de diseño que nos sirven.**
- **S12:** el bloque de C (en nuestro caso, el bloque K·U local) debe caber
  en la memoria de la GPU; si no, hay que trocearlo. Para el simulador, K y U
  locales caben de sobra en una GPU de 6 GB, así que podemos dejar K
  **residente en la GPU** todo el tiempo y mover solo las fronteras.
- **S13:** solapar el intercambio de fronteras con el cómputo interior es la
  misma idea que su lookahead/precarga.
- Medir con datos en la memoria del host y contar la transferencia host–GPU
  dentro del tiempo (ellos lo hacen así). Lo adoptamos para que el speedup de
  CUDA sea honesto.

**4. Cómo miden.**
**Summit** (ORNL): 4600 nodos IBM AC922, cada uno con 2 CPU POWER9 y
**6 GPU NVIDIA V100**. GEMM local con cuBLAS; pico práctico medido: 7.2 TFLOP/s
por GPU. Cada punto: 5 a 10 ejecuciones. Mallas de procesos de 2 × 2
(12 GPU) a 12 × 12 (432 GPU), con 2 procesos PaRSEC por nodo.

**5. Resultado principal.**
En un nodo, con tiles de 1024 o más, **más de 95 % del pico práctico** con 1 a
3 GPU y **85 %** con 4 a 6 GPU (la caída la atribuyen al hardware: bus entre
GPU y RAM según el banco NUMA). En la conclusión reportan alrededor de 85 % del
pico incluso con matrices que **no caben** en la memoria sumada de las GPU.
Dato curioso: cerca del 50 % de las cargas de tiles fueron de GPU a GPU, no
desde la RAM.

**6. Qué NO aplica a nuestro caso.**
Varias GPU por nodo, cientos de GPU y un runtime de tareas (PaRSEC) están
fuera de nuestro alcance: tendremos **una GPU** (esta PC tiene una GTX 1660
SUPER; también está Colab) y pocos procesos MPI. Además su caso es denso y el
nuestro (K·U) es disperso: la intensidad aritmética es mucho menor y el
cuello de botella será el ancho de banda de memoria, no los TFLOP/s.

**7. Dudas para discutir.**
- ¿Usamos cuSPARSE como referencia de K·U en GPU (lo dice el cronograma, S12)?
- ¿Cuánto pesa la transferencia host–GPU por paso si solo movemos fronteras?

Fuente: [PDF en ICL](https://icl.utk.edu/files/publications/2019/icl-utk-1268-2019.pdf) ·
[ORNL](https://impact.ornl.gov/en/publications/generic-matrix-multiplication-for-multi-gpu-accelerated-distribut/)
