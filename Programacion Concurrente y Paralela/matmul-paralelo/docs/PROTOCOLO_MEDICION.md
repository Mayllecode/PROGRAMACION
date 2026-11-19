# Protocolo de medición

Objetivo: que cualquier número del informe final sea **reproducible** y que
los tiempos de distintas versiones sean **comparables** entre sí.

## 1. Antes de medir

1. `make test` pasa. Una versión que no pasa la verificación no se mide.
2. Siempre en la **misma máquina oficial** para una misma tabla o gráfico.
   Se adjunta su `results/entorno.txt`.
3. Máquina conectada a la corriente, sin otros programas pesados abiertos
   (navegador, IDE indexando, videollamada).
4. Mismas banderas de compilación para todas las versiones (`-O2`, definidas
   en el `Makefile`). Si se cambian, se vuelven a medir **todas**.

## 2. Cómo se mide

- 1 ejecución de calentamiento por caso (no se cuenta) que además se verifica.
- **5 repeticiones** medidas por cada punto (algoritmo, n, p).
- Se reporta la **mediana**, no el promedio: una sola ejecución interrumpida
  por el sistema operativo no arrastra el resultado.
- En el informe se reporta también la dispersión (mínimo y máximo).
- Se mide solo el cómputo de C = A·B. Generar matrices, reservar memoria y
  verificar quedan fuera del cronómetro. En MPI (S9) se medirán aparte el
  tiempo de cómputo y el de comunicación.

## 3. Tamaños de prueba

| Caso | Tamaños | Nota |
|---|---|---|
| Denso | n = 256, 512, 1024, 2048 | naive solo hasta 1024 |
| Disperso (rigidez 2D) | malla de 128², 256², 512² nodos, 16 vectores | n = 16 384 a 262 144 |

Se pueden agregar tamaños mayores si la máquina lo permite, pero estos son
los mínimos que deben aparecer en todas las fases para poder compararlas.

## 4. Métricas

- **T(p):** tiempo (mediana) con p hilos o procesos.
- **T(1):** tiempo de la **mejor versión secuencial** (hoy: `ikj` o por
  bloques, la más rápida en la máquina oficial), no de la versión naive.
  Comparar contra naive infla el speedup con una mejora que no viene del
  paralelismo sino del orden de los bucles.
- **Speedup:** S(p) = T(1) / T(p).
- **Eficiencia:** E(p) = S(p) / p.
- **GFLOP/s:** 2n³ / T para denso; 2 · nnz · k / T para disperso.
- **Comunicación (desde S10):** bytes enviados y número de mensajes por fase.

## 5. Escalabilidad

- **Fuerte:** n fijo, p = 1, 2, 4, 8 (…). ¿Cuánto baja el tiempo?
- **Débil:** el trabajo por proceso se mantiene constante al crecer p.
  Ojo: en multiplicación densa el trabajo crece como **n³**, así que mantener
  n/p constante *no* mantiene el trabajo por proceso constante. Para trabajo
  constante: n = n₀ · p^(1/3). Si lo que se quiere mantener constante es la
  memoria por proceso: n = n₀ · √p. Hay que declarar en el informe cuál de
  los dos criterios se usó.

## 6. Formato de los datos

Todo resultado se guarda en CSV en `results/`, con esta cabecera:

```
algoritmo,n,repeticion,tiempo_s,gflops
```

En fases paralelas se agregan columnas `p` (hilos o procesos) y, en MPI,
`t_comp_s,t_comm_s,bytes,mensajes`.

El simulador completo (`bench/bench_sim.c`) usa las mismas cinco primeras
columnas y agrega `pisos,vanos,k,pasos,t_kernel_s,frac_kernel`. `frac_kernel`
es la fracción f del tiempo dentro de K·U: con ella se calcula el límite de
Amdahl 1/(1 − f) si solo se paraleliza K·U. Nombre del archivo:
`<fase>_<máquina>_<fecha>.csv`, por ejemplo `omp_labpc3_2026-10-08.csv`.
