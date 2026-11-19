# Hito H1 — Línea base y simulador validados (fin de la semana 6)

**Criterio de aceptación** (diapositiva 10 del cronograma):

> El secuencial pasa la suite de verificación y el simulador reproduce la
> vibración libre de un oscilador de 1 GDL, con periodo T = 2π√(m/k).

| Parte del criterio | Evidencia | Resultado |
|---|---|---|
| El secuencial pasa la suite de verificación | `make test` → `tests/test_seq.c` | **47 / 47 OK** |
| El simulador está validado | `make test` → `tests/test_sim.c` | **23 / 23 OK** |
| Reproduce la vibración libre de 1 GDL | prueba 1 y figura `01_validacion_1gdl.png` | error máx. 1.2·10⁻⁴ en 5 periodos (ωΔt = 0.01) |
| …con periodo T = 2π√(m/k) | prueba 1b: periodo **medido** en la simulación | 0.314156 s vs 0.314159 s → error **1.0·10⁻⁵** |
| Línea base T(1) | `make bench` → `results/t1_*.csv`, figuras 06 y 07 | tablas abajo |

**Estado: H1 cumplido** en la máquina DESKTOP-R3PLOD9 (Windows 10, gcc 16.2
WinLibs, `-O2`), 2026-10-01. Falta repetir `make test` y `make bench` en la
**máquina oficial** que elija el equipo (tarea S5-6) y en la de cada integrante.

---

## 1. Verificación del producto de matrices (47 pruebas)

| Grupo | Qué demuestra |
|---|---|
| Ejemplo a mano `[2x3]·[3x2] = [[58,64],[139,154]]` | resultado exacto, comprobable en papel (5 variantes) |
| Identidad, n = 37 | A·I = I·A = A **bit a bit**; n impar prueba los bordes de los bloques |
| Permutación | detecta índices cruzados; Pᵀ·P = I |
| Consistencia vs. naive | ikj y bloques de 7, 8 y 32 con matrices rectangulares |
| Disperso CSR | tridiagonal a mano, simetría de K, K·1, ida y vuelta CSR ↔ densa, SpMM vs. denso |
| Reproducibilidad | misma semilla → misma matriz en cualquier máquina |
| El verificador | acepta 10⁻¹⁴, rechaza 10⁻³ y NaN: si no detectara errores, las otras pruebas no valdrían |

Tolerancia: |x − ref| ≤ 10⁻⁹·(1 + |ref|). No se exige igualdad exacta porque
las versiones paralelas sumarán en otro orden (la suma en punto flotante no es
asociativa); un error de programación real da diferencias de orden 1.

## 2. Validación del simulador (23 pruebas)

| Prueba | Esperado | Obtenido |
|---|---|---|
| 1 GDL sin amortiguar, 5 periodos | u(t) = cos(ωt) | error 1.24·10⁻⁴ |
| 1 GDL con amortiguamiento (α = 2 s⁻¹) | solución analítica amortiguada | error 3.17·10⁻⁵ |
| Orden de convergencia | error(Δt) / error(Δt/2) ≈ 4 | **4.00** (6 refinamientos, fig. 02) |
| **Periodo medido (H1)** | T = 2π√(m/k) = 0.314159 s | 0.314156 s (error 1.0·10⁻⁵ = (ωΔt)²/24, lo que predice la teoría) |
| Paso crítico 1 GDL | 2/ω = 0.1 s | exacto |
| Modelo del edificio | K simétrica, K·1 = kc en el piso 1, enlaces al nodo de abajo | OK |
| T1 del edificio (10 pisos) | fórmula cerrada del edificio de cortante | 1.004929 s, error < 10⁻¹² |
| Sismo constante + amortiguamiento fuerte | equilibrio K·u = −M·1·a | residuo 1.2·10⁻¹⁴ |
| Linealidad | resp(A + B) = resp(A) + resp(B) | error 6.9·10⁻¹⁶ |
| Escenarios juntos vs. separados | idénticos | **bit a bit** |
| Estabilidad con 0.95·Δt crítico, 20 000 pasos | respuesta acotada | máx. 1.42 (acotado) |
| Lectura de acelerogramas | cabecera ignorada, columna elegida, unidades, re-muestreo | 6 pruebas OK |

## 3. Línea base T(1) — producto denso C = A·B

Mediana de 5 repeticiones (mín – máx), tras 1 ejecución de calentamiento verificada.

| n | naive | ikj | bloques 64 | mejor (= T(1)) |
|---:|---:|---:|---:|---|
| 256 | 0.0148 s (0.0143–0.0150) | 0.0076 s (0.0075–0.0077) | 0.0076 s (0.0074–0.0079) | empate ikj / bloques |
| 512 | 0.1663 s (0.1591–0.1678) | **0.0485 s** (0.0480–0.0490) | 0.0548 s (0.0544–0.0557) | ikj |
| 1024 | 1.4865 s (1.3879–1.5398) | 0.4739 s (0.4428–0.4818) | **0.4448 s** (0.4428–0.4514) | bloques 64 |
| 2048 | no se mide | 4.1725 s (4.1607–4.1918) | **3.5383 s** (3.5317–3.5424) | bloques 64 |

- **Solo cambiar el orden de los bucles** (naive → ikj) da **3.1×** en n = 1024
  (entre 3.1× y 3.5× en las dos corridas que hicimos, porque naive es la
  versión más ruidosa), sin paralelismo: es efecto de la caché. Por eso el speedup de S7 se calcula
  contra la mejor versión secuencial y no contra naive.
- Rendimiento secuencial: ~4.5–4.8 GFLOP/s.
- En n = 2048 los bloques ganan a ikj por 1.18×. Explicación probable: ikj
  recorre B entera (32 MB) por cada fila de A y B ya no cabe en la caché L3
  de 12 MB; los bloques reutilizan cada trozo de B mientras está en caché.
  El tamaño de bloque óptimo se busca en S7.

**Disperso** (rigidez 2D, 16 vectores): 0.0006 s (n = 16 384), 0.0026 s
(n = 65 536), 0.0102 s (n = 262 144); ~4.0–4.1 GFLOP/s.

## 4. Línea base T(1) — simulador completo

400 pasos, 8 escenarios, mediana de 5.

| Edificio (pisos × vanos) | n (GDL) | nnz(K) | tiempo total | en K·U | fracción K·U | máx. speedup si solo se paraleliza K·U |
|---|---:|---:|---:|---:|---:|---:|
| 10 × 4 | 50 | 220 | 0.0009 s | 0.0003 s | 39 % | 1.6× |
| 20 × 50 | 1 020 | 4 958 | 0.0167 s | 0.0077 s | 46 % | 1.9× |
| 50 × 200 | 10 050 | 49 748 | 0.1663 s | 0.0801 s | 48 % | 1.9× |
| 100 × 400 | 40 100 | 199 498 | 0.7418 s | 0.3472 s | 47 % | 1.9× |

**Consecuencia para S7:** el producto K·U es solo la mitad del tiempo. Por la
ley de Amdahl, si OpenMP paraleliza solo K·U el speedup no pasa de ~1.9× con
cualquier número de hilos. Hay que paralelizar también la actualización de U
y el cálculo de picos (los dos bucles sobre nodos de `sim_run`).

## 5. Cómo reproducir todo

```bash
make entregable      # Windows: mingw32-make entregable (desde Git Bash)
```

Genera en `results/`: `entorno.txt`, `t1_<máquina>.csv`, `t1_sim_<máquina>.csv`,
`validacion_1gdl.csv`, `convergencia_1gdl.csv`, `respuesta.csv`,
`mapa_deriva.csv`, `historia_techo.csv`, `demo_salida.txt` y las 7 figuras de
`results/figuras/`.
