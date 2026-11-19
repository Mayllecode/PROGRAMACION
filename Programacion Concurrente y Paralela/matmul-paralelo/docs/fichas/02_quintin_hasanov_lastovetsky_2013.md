# Ficha de lectura 2 — Quintin, Hasanov y Lastovetsky (ICPP 2013): HSUMMA

```
Artículo:    J.-N. Quintin, K. Hasanov y A. Lastovetsky, "Hierarchical Parallel Matrix
             Multiplication on Large-Scale Distributed Memory Platforms", 42nd
             International Conference on Parallel Processing (ICPP 2013), IEEE,
             pp. 754-762. Preprint: arXiv:1306.4161   (University College Dublin)
Responsable: C
Fecha:       2026-10-01 (borrador)
```

> **Estado:** borrador hecho con el **texto completo** del preprint de arXiv
> (público). Las cifras están copiadas del artículo; el responsable debe
> revisarlo y reescribir con sus palabras lo que vaya a exponer.

**1. Problema que resuelven.**
Cannon (1969) tiene costo de comunicación teóricamente óptimo pero exige un
número cuadrado de procesadores; SUMMA (mediados de los 90) funciona con
cualquier malla p = s × t. Al crecer el número de procesadores en dos órdenes
de magnitud, la **comunicación** pasó a dominar el tiempo, y SUMMA hay que
revisarlo para reducirla.

**2. Idea principal.**
**HSUMMA**: la misma malla de SUMMA, organizada en **dos niveles**. Los
procesadores se agrupan en G grupos (una malla I × J de grupos, cada uno con
una submalla de s/I × t/J). Cada difusión (broadcast) de SUMMA se parte en dos
fases: primero **entre grupos** y después **dentro de cada grupo**. Puede usar
un tamaño de bloque B entre grupos y otro b ≤ B dentro del grupo; el número
total de pasos y la cantidad de datos enviados son los mismos que en SUMMA.

- SUMMA básico: en cada uno de los n/b pasos, quien tiene parte de la
  "columna pivote" de A la difunde **horizontalmente** por su fila de
  procesadores, quien tiene parte de la "fila pivote" de B la difunde
  **verticalmente** por su columna, y cada uno actualiza su bloque de C.
- **SUMMA es un caso particular de HSUMMA** con G = 1 o G = p.

**3. Decisiones de diseño que nos sirven.**
- **S10 (SUMMA):** implementar el broadcast por filas y columnas con
  comunicadores MPI de fila y de columna (`MPI_Comm_split`), tal como lo
  describen. Medir comunicación aparte del cómputo, como hacen ellos.
- **S13 (HSUMMA, opcional):** sobre nuestra SUMMA solo cambia la
  organización de los broadcasts; el análisis dice que el extremo de su costo
  está en **G = √p** (puede ser mínimo o máximo según la latencia α y el ancho
  de banda β de la red; si es máximo, lo mejor es G = 1 o G = p, o sea SUMMA).
  Conclusión práctica de los autores: el G óptimo se puede encontrar probando
  pocas iteraciones con distintos G.

**4. Cómo miden.**
- **Grid'5000**, clúster Graphene (Nancy): MPICH-2 (también probaron
  Open MPI, con resultados similares), Intel MKL BLAS; n = 8192, p = 128,
  bloques b = B = 64 y 512.
- **BlueGene/P Shaheen** (KAUST): nodos de 4 núcleos PowerPC 450 a 850 MHz,
  4 GB; ESSL DGEMM; n = 65 536, b = B = 256, hasta 16 384 núcleos.
- Tiempos = **promedio de 30 ejecuciones**. Reportan tiempo total y tiempo
  de comunicación por separado.

**5. Resultado principal.**
En 16 384 núcleos (BG/P): SUMMA tarda 50.2 s (36.46 s de comunicación);
HSUMMA con G = 512 tarda 21.26 s (6.19 s de comunicación). Es decir,
**5.89× menos tiempo de comunicación y 2.36× menos tiempo total**. En 2048
núcleos: 2.08× menos comunicación y 1.2× menos tiempo total. **Con menos de
2048 núcleos, HSUMMA y SUMMA rindieron casi igual.**

**6. Qué NO aplica a nuestro caso.**
Tendremos de 4 a 16 procesos MPI: según sus propios datos, a esa escala la
mejora es despreciable. HSUMMA es interesante como diseño, no como fuente de
speedup para nosotros. **Ojo al citar el 5.89×:** es reducción del tiempo de
**comunicación**, con 16 384 núcleos; no es el speedup total.

**7. Dudas para discutir.**
- ¿Vale la pena HSUMMA en S13, o es mejor dedicar ese tiempo al híbrido
  MPI + CUDA?
- ¿Cómo agrupar 16 procesos (G = 4 = √16) y medir si cambia algo?

Fuente: [arXiv:1306.4161](https://arxiv.org/abs/1306.4161) ·
[IEEE Xplore](https://ieeexplore.ieee.org/document/6687414/)
