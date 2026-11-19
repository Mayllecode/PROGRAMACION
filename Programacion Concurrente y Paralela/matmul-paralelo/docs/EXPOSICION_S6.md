# Guion de la exposición — Fase 1 (semanas 5 y 6)

Duración sugerida: **12–15 minutos + preguntas**. Todo lo que se muestra ya
está generado en `results/` y `results/figuras/`; la demo en vivo es opcional
(ver §3, plan B).

## 1. Mensaje central (decirlo al inicio y al final)

> Construimos el **simulador secuencial** de la respuesta sísmica de un edificio
> y lo **validamos contra soluciones exactas**. Su núcleo es el producto de una
> matriz dispersa por una matriz densa, **K·U**, que en las siguientes fases
> paralelizamos con OpenMP, MPI y CUDA. Ya medimos la **línea base T(1)** y
> sabemos qué hay que paralelizar en la semana 7.

## 2. Orden sugerido

| # | Tema | Qué mostrar | Idea que debe quedar |
|---|---|---|---|
| 1 | Problema y propuesta | diapositivas 2–3 del cronograma | Simular un edificio = miles de productos K·U; cada columna de U es un sismo |
| 2 | Modelo físico | `docs/MODELO_FISICO.md` §2–3 | Pórtico de pisos × vanos, M diagonal → **método explícito**, sin resolver sistemas: el único trabajo pesado es K·U |
| 3 | Estabilidad | §3 | Δt < 2/ω_max; el código calcula una cota segura (Gershgorin) y usa la mitad |
| 4 | Código | `include/sim.h` | El kernel K·U es **enchufable** (`spmm_fn`): en S7 se cambia `spmm_csr` por la versión OpenMP sin tocar el simulador |
| 5 | Verificación | `make test` → **70 pruebas OK** | Resultados conocidos de antemano + consistencia + el verificador detecta errores |
| 6 | **Hito H1** | fig. `01_validacion_1gdl.png`, `02_convergencia.png` | Periodo medido 0.314156 s vs 2π√(m/k) = 0.314159 s; el error cae 4× al dividir Δt entre 2 (segundo orden) |
| 7 | Demo sísmica | `make demo`, fig. `03`, `04`, `05` | 4 sismos de 0.3 g a la vez; el de 1 Hz entra en **resonancia** (T1 = 1.0 s) y mueve el techo **6×** más |
| 8 | Línea base T(1) | fig. `06_t1_denso.png` | Reordenar bucles da ~3× sin paralelismo → el speedup se mide contra la **mejor** versión secuencial |
| 9 | Qué sigue (S7) | fig. `07_t1_simulador.png` | K·U es ~47 % del tiempo: si solo se paraleliza K·U, **Amdahl limita a ~1.9×** |
| 10 | Lectura IEEE | `docs/fichas/` | Una frase por artículo y en qué semana lo usamos (§5) |
| 11 | Límites del modelo | `MODELO_FISICO.md` §7 | Lineal, elástico, 2D: no reemplaza la Norma E.030 ni a un ingeniero |

## 3. Demo en vivo (Git Bash en la carpeta del proyecto)

```bash
mingw32-make test     # Linux/WSL: make test
mingw32-make demo
```

**Salida esperada de `make test`** (final de cada suite):

```
=== Resultado: 47 pruebas OK, 0 fallas ===
...
1b) Hito H1: periodo medido en la simulacion vs T = 2*pi*sqrt(m/k) = 0.314159 s
  [OK]    periodo medido en 19 ciclos = 0.314156 s, error relativo 1.0e-05 < 1e-4
...
=== Resultado: 23 pruebas OK, 0 fallas ===
```

**Salida esperada de `make demo`** (resumida; completa en `results/demo_salida.txt`):

```
Edificio: 10 pisos x 4 vanos -> 50 grados de libertad, nnz(K) = 220
Periodo fundamental: T1 = 1.005 s (f1 = 1.00 Hz) | dt = 0.0061 s (critico 0.0122 s) | 4929 pasos (30 s)

Resumen por escenario
  escenario    desp. techo [cm]   deriva max  piso critico
  0.5 Hz                  14.73       0.0084             1
  1.0 Hz                  88.59       0.0441             1
  2.0 Hz                   4.23       0.0021             6
  4.0 Hz                   0.12       0.0011             8

Resonancia: el sismo de 1.0 Hz es el mas cercano a f1 = 1.00 Hz y mueve el techo
6.0 veces mas que el siguiente escenario sintetico (0.5 Hz).
```

Cómo leerla:
- **T1 = 1.0 s** cumple la regla práctica "0.1 s por piso" para 10 pisos.
- **Resonancia:** el sismo cuya frecuencia coincide con la del edificio produce
  88.6 cm en el techo, contra 14.7 cm del de 0.5 Hz, con la **misma**
  aceleración pico. Una senoide pura en resonancia es un caso extremo, más
  severo que un sismo real.
- **Piso crítico:** con 0.5 y 1 Hz la mayor deriva está en el piso 1 (primer
  modo); con 2 y 4 Hz sube a los pisos 6 y 8 porque esas frecuencias excitan los
  **modos altos** del edificio (el segundo modo está cerca de 3 Hz).

**Plan B** si la PC no coopera: mostrar las figuras de `results/figuras/` y
`results/demo_salida.txt`, que son la misma corrida.

## 4. Preguntas probables

**¿Por qué diferencias centrales y no Newmark u otro método implícito?**
Porque con M diagonal es explícito: cada paso es un K·U y operaciones por nodo,
sin resolver sistemas lineales. Eso es justo lo que se paraleliza bien. El
costo es la condición de estabilidad Δt < 2/ω_max, que el código respeta.

**¿Cómo saben que el simulador está bien?**
Lo comparamos con casos de solución exacta: el oscilador de 1 GDL (error 10⁻⁴
y periodo medido con error 10⁻⁵), el orden de convergencia (4.00, la teoría
dice 4), el equilibrio estático (residuo 10⁻¹⁴), la linealidad y que varios
escenarios juntos dan bit a bit lo mismo que por separado.

**¿Por qué el speedup se calculará contra ikj/bloques y no contra naive?**
Porque naive → ikj ya da ~3× (3.1–3.5× según la corrida) sin ningún paralelismo. Contar eso como speedup
paralelo sería inflar el resultado.

**¿Por qué varios sismos a la vez?**
Porque así K·U es una matriz dispersa por una matriz densa de k columnas (SpMM)
en vez de k productos matriz-vector: K se lee una sola vez de memoria por paso
para todos los escenarios. Es el kernel del artículo de Huang & Chow.

**¿Las derivas cumplen la Norma E.030?**
El modelo es lineal y elástico, y la E.030 limita derivas de diseño
(inelásticas; 0,007 para concreto armado). La línea de 0,007 en la figura 03 es
solo una referencia visual, no una verificación normativa.

**¿Qué falta?**
Las tareas de equipo: repositorio en GitHub, `make env` de cada integrante, la
máquina oficial de medición, un acelerograma peruano real en `data/` y la
lectura completa de los artículos (las fichas están en borrador).

## 5. Los tres artículos en una frase

- **Huang & Chow (TPDS 2024):** con varios vectores, repartir también las
  columnas del bloque de vectores puede reducir la comunicación de SpMM →
  S11, reparto de la estructura y de los escenarios.
- **Quintin et al. (ICPP 2013), HSUMMA:** agrupar los procesos de SUMMA en dos
  niveles reduce la comunicación (5.89× en 16 384 núcleos; casi nada por debajo
  de 2048) → S10 SUMMA y S13 opcional.
- **Herault et al. (ScalA 2019), PaRSEC:** GEMM multi-GPU troceando C para que
  quepa en la GPU y precargando datos (lookahead): 85 % del pico en Summit →
  S12–S13, K·U en la GPU y solapamiento.
