# Multiplicación de Matrices Paralelas — UNMSM

Proyecto 2 del curso **Programación Concurrente y Paralela**.
Kernel: `C = A · B`, denso y disperso (CSR), aplicado a matrices de rigidez
para análisis sísmico estructural.

**Propuesta:** un simulador de la respuesta sísmica de edificios. La estructura
se representa con una matriz de rigidez K y la simulación avanza en el tiempo
multiplicando K por los desplazamientos de todos los escenarios a la vez (K·U).
Ese producto es el que se paraleliza con OpenMP, MPI y CUDA.

Este repositorio cubre la **Fase 1 — Fundamentos y modelo (semanas 5 y 6)**:
producto de matrices secuencial de referencia, simulador sísmico secuencial
validado, suites de verificación y línea base T(1); y la **semana 7 de la
Fase 2 — Memoria compartida**: OpenMP (producto denso, K·U y simulador
completo) y la variante en Go con un *worker pool* de goroutines; y la **Fase 3 — Memoria
distribuida (semanas 8 a 11)**: diseño del reparto, Cannon, SUMMA y el
simulador con la estructura repartida entre procesos MPI (Hito H3).

| Documento | Para qué |
|---|---|
| [`docs/GUIA_S9_S11.md`](docs/GUIA_S9_S11.md) | **Semanas 9 a 11 (MPI)**: instalar MPI, compilar, probar con 1 a 16 procesos, medir y la demo |
| [`docs/DISENO_REPARTO_MPI.md`](docs/DISENO_REPARTO_MPI.md) | **Semana 8**: cómo se reparten las matrices y el edificio entre procesos, y por qué |
| [`docs/GUIA_S7.md`](docs/GUIA_S7.md) | **Semana 7 (OpenMP y Go)**: compilar, probar, medir S(p) y E(p), demo y valores de referencia |
| [`docs/HITO_H1.md`](docs/HITO_H1.md) | **Evidencia del Hito H1**: criterio, pruebas y tablas de T(1) |
| [`docs/EXPOSICION_S6.md`](docs/EXPOSICION_S6.md) | Guion de la exposición, salida esperada y preguntas probables |
| [`docs/INSTALACION.md`](docs/INSTALACION.md) | Instalación en Windows, WSL2 y Colab; CUDA; problemas frecuentes |
| [`docs/PLAN_S5_S6.md`](docs/PLAN_S5_S6.md) | Tareas de las semanas 5 y 6, responsables y estado |
| [`docs/MODELO_FISICO.md`](docs/MODELO_FISICO.md) | Modelo del edificio, ecuación, método, validación y límites |

---

## 1. Qué hace el programa

1. **Arma el edificio**: un pórtico 2D de `pisos × vanos`, con un nodo (masa)
   en cada cruce piso–columna. Su matriz de rigidez K es simétrica y dispersa
   (máximo 5 no nulos por fila) y se guarda en formato CSR.
2. **Arma los escenarios**: U es una matriz n × k, **una columna por sismo**
   (4 sintéticos de 0.3 g a 0.5, 1, 2 y 4 Hz, más un registro real opcional).
3. **Avanza en el tiempo** con diferencias centrales (método explícito). En cada
   paso el único trabajo pesado es **K·U**: un solo producto mueve todos los
   sismos a la vez. Ese producto es el que se paraleliza en las fases siguientes.
4. **Reporta**: desplazamiento máximo y deriva de cada piso por escenario, el
   piso crítico, la resonancia, la historia del techo y el mapa de demanda.

## 2. Estructura

```
include/            Cabeceras
  matrix.h          Matriz densa (row-major) y dispersa (CSR)
  generators.h      Matrices de prueba reproducibles (semilla explícita)
  seq.h             Versiones secuenciales: naive, ikj, por bloques, SpMM CSR
  verify.h          Comparación contra la referencia con tolerancia ε
  timer.h           Reloj monotónico (Linux y Windows)
  building.h        Modelo del edificio: K, masas, pisos, periodo fundamental
  ground_motion.h   Sismos sintéticos y lectura robusta de acelerogramas reales
  sim.h             Simulador (diferencias centrales); el kernel K·U es enchufable
src/                Implementaciones de lo anterior
tests/test_seq.c    Verificación del producto de matrices (47 pruebas)
tests/test_sim.c    Validación del simulador contra soluciones exactas (23 pruebas)
apps/demo_sismo.c   Edificio ante varios sismos: tablas + CSV para gráficos
apps/validar_1gdl.c Oscilador de 1 GDL: curvas simulado vs. exacto y convergencia
bench/bench_seq.c   T(1) del producto denso y disperso → CSV
bench/bench_sim.c   T(1) del simulador completo, fracción de tiempo en K·U → CSV
scripts/check_env.sh  Revisa gcc, OpenMP, Python, Go, MPI y CUDA (Linux y Git Bash)
scripts/graficos.py   Figuras del entregable (matplotlib) → results/figuras/
data/               Acelerogramas reales y su fuente (ver data/README.md)
docs/               Plan, modelo, protocolo, guía de lectura, fichas, actas, H1, exposición
results/            CSV de mediciones, salida de la demo, reporte de entorno y figuras
```

**Semana 7 (memoria compartida)** — detalle en [`docs/GUIA_S7.md`](docs/GUIA_S7.md):

```
include/par_omp.h       Versiones OpenMP: producto denso, K·U y simulador completo
src/omp/matmul_omp.c    matmul_omp_ikj, matmul_omp_blocked, spmm_csr_omp
src/omp/sim_omp.c       sim_run_omp: K·U + actualización de U + picos en paralelo
tests/test_omp.c        51 pruebas: idéntico bit a bit al secuencial con 1, 2, 3, 4 y 8 hilos
bench/bench_omp.c       S(p) y E(p) del producto denso y disperso → CSV
bench/bench_sim_omp.c   S(p) y E(p) del simulador: solo K·U vs. todo el paso (Amdahl) → CSV
go/                     Versión Go: worker pool de goroutines, pruebas (go test) y medición
scripts/graficos_s7.py  Figuras 08 y 09: speedup y eficiencia
```

```bash
make omp && make test-omp   # 51 pruebas OK
make go-test                # pruebas de Go
make bench-omp go-bench     # S(p) y E(p) en results/ (unos 7 minutos)
make figuras-s7             # results/figuras/08 y 09
make demo-omp               # la demo con el simulador OpenMP
```

**Semanas 9 a 11 (memoria distribuida)** — detalle en [`docs/GUIA_S9_S11.md`](docs/GUIA_S9_S11.md):

```
include/par_mpi.h        API MPI: Cannon, SUMMA, reparto, K·U y simulador distribuidos
src/mpi/cannon.c         Cannon: malla q x q periódica, sesgo inicial y rotaciones
src/mpi/summa.c          SUMMA: malla pr x pc (cualquier p), difusiones por fila y columna
src/mpi/sim_mpi.c        Reparto en franjas de pisos (por filas o por nnz), nodos fantasma,
                         intercambio de fronteras y el simulador distribuido
src/mpi/mpi_common.c     Mallas de procesos, reparto de bloques, contadores de mensajes y bytes
tests/test_mpi.c         Cannon/SUMMA dentro de ε; K·U y simulador idénticos bit a bit (p = 1..16)
bench/bench_mpi.c        Tiempo, mensajes y bytes de Cannon y SUMMA → CSV
bench/bench_sim_mpi.c    S(p) del simulador repartido (por filas y por nnz) → CSV
scripts/graficos_s11.py  Figuras 10, 11 y 12 y tablas de S(p) y E(p)
apps/carrera_mpi.c       La carrera: mismo edificio con 1 a p procesos, speedup y resultado idéntico
DEMO_MPI.bat         La demo en vivo del Hito H3, paso a paso (la de S7 sigue en DEMO_EN_VIVO.bat)
```

```bash
make mpi && make test-mpi   # pruebas con 1, 2, 4, 6, 9 y 16 procesos
make bench-mpi              # Cannon, SUMMA y simulador (unos 15 minutos)
make figuras-s11
make demo-mpi               # la demo con 4 procesos (mismos CSV que make demo)
make carrera-mpi            # el edificio grande con 1, 2, 4 y 9 procesos: 4.7 s -> 1.4 s
```

Carpeta que se agregará en la fase siguiente: `src/cuda/` (S12–S13).

## 3. Instalación (resumen; detalle en [`docs/INSTALACION.md`](docs/INSTALACION.md))

**Linux o WSL2:**

```bash
sudo apt update
sudo apt install build-essential git openmpi-bin libopenmpi-dev golang-go python3-matplotlib
```

**Windows nativo** (todo hasta S11, MPI incluido con MS-MPI):

```powershell
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT   # gcc 16 + mingw32-make + OpenMP
winget install -e --id Microsoft.msmpi                    # MPI: mpiexec (pide admin)
winget install -e --id Microsoft.msmpisdk                 # MPI: mpi.h y msmpi.lib
pip install matplotlib
```

MS-MPI y OpenMPI implementan el mismo estándar: el código de `src/mpi/` es
el mismo; solo cambia cómo se compila (lo resuelve el `Makefile`).

y trabajar desde **Git Bash** usando `mingw32-make` en lugar de `make`.

**CUDA:** esta PC tiene una GTX 1660 SUPER (ver la guía); si no hay GPU, Google Colab.

Luego, **cada integrante** corre `make env`: revisa todo y guarda
`results/entorno.txt` (renombrarlo a `entorno_<nombre>.txt` antes de subirlo).

## 4. Uso

```bash
make test        # 47 + 23 pruebas, todas OK
make validar     # oscilador de 1 GDL: curvas y convergencia (CSV)
make demo        # edificio de 10 pisos x 4 vanos ante 4 sismos sintéticos
make bench       # T(1) del producto de matrices y del simulador (unos minutos)
make bench-sim   # solo el simulador (segundos)
make figuras     # 7 gráficos en results/figuras/
make entregable  # todo lo anterior, en orden
make clean
```

`make bench` corre primero las pruebas: si la verificación falla, no mide nada.

Demo con un registro real (detalle de formatos y unidades en [`data/README.md`](data/README.md)):

```bash
#                   pisos vanos archivo           dt     factor a m/s²  [columna]
./build/demo_sismo  10    4     data/registro.txt 0.01   0.01           1
```

## 5. Qué debe salir

| Comando | Salida en pantalla | Archivos |
|---|---|---|
| `make test` | `47 pruebas OK, 0 fallas` y `23 pruebas OK, 0 fallas` | — |
| `make validar` | tabla de convergencia con razón **4.00** en cada fila | `validacion_1gdl.csv`, `convergencia_1gdl.csv` |
| `make demo` | T1 = 1.005 s; tablas por piso; resumen por escenario; aviso de **resonancia** a 1 Hz (6× más desplazamiento) | `respuesta.csv`, `mapa_deriva.csv`, `historia_techo.csv`, `demo_salida.txt` |
| `make bench` | medianas y GFLOP/s por algoritmo y tamaño; % de tiempo en K·U | `t1_<máquina>.csv`, `t1_sim_<máquina>.csv` |
| `make figuras` | 7 líneas `[OK]` | `results/figuras/01…07_*.png` |

| Figura | Muestra |
|---|---|
| `01_validacion_1gdl.png` | Simulación vs. solución exacta del oscilador de 1 GDL (Hito H1) |
| `02_convergencia.png` | Error ÷ 4 al dividir Δt entre 2: método de segundo orden |
| `03_respuesta_por_piso.png` | Desplazamiento y deriva por piso para cada sismo |
| `04_historia_techo.png` | a_g(t) y desplazamiento del techo: misma PGA, respuestas muy distintas |
| `05_mapa_demanda.png` | Pórtico coloreado por deriva + mapa piso × escenario |
| `06_t1_denso.png` | T(1) del producto denso: naive vs. ikj vs. bloques |
| `07_t1_simulador.png` | Fracción del tiempo en K·U y límite de Amdahl para S7 |

## 6. Qué verifica la suite

| Grupo | Qué prueba | Por qué importa |
|---|---|---|
| Ejemplo a mano | `[2x3]·[3x2]` da `[[58,64],[139,154]]` exacto | Resultado que cualquiera puede comprobar en papel |
| Identidad | `A·I = A` e `I·A = A`, n = 37 | n impar, no múltiplo del bloque: prueba los bordes |
| Permutación | Filas reordenadas exactamente; `Pᵀ·P = I` | Detecta índices cruzados |
| Consistencia | ikj y bloques (7, 8, 32) contra naive, matrices rectangulares | Todas las variantes calculan lo mismo |
| Disperso | Tridiagonal a mano, simetría de la rigidez, `K·1`, CSR ida y vuelta, SpMM vs. denso | Base del caso sísmico |
| Reproducibilidad | Misma semilla → misma matriz | Cualquiera reproduce el caso de otro |
| El verificador | Acepta 1e-14, rechaza 1e-3 y NaN | Si no detecta errores, las demás pruebas no valen |
| Oscilador de 1 GDL | Solución exacta, con y sin amortiguamiento; orden 2 | El simulador resuelve bien la física |
| **Periodo (H1)** | Periodo **medido** = 2π√(m/k), error 1e-5 | Criterio literal del Hito H1 |
| Edificio | K simétrica, equilibrio de resortes, T1 = fórmula cerrada | El modelo está bien armado |
| Equilibrio, linealidad, escenarios | Residuo 1e-14; A+B; juntos = separados bit a bit | Propiedades que toda versión paralela debe conservar |
| Estabilidad | 0.95·Δt crítico, 20 000 pasos, acotado | El paso de tiempo elegido es seguro |
| Acelerogramas | Cabecera ignorada, columna, unidades, re-muestreo, errores | Un registro real se lee bien o falla con un mensaje claro |

**Regla del equipo:** nada entra a `main` si `make test` no pasa. Cuando lleguen
OpenMP, MPI y CUDA, cada versión se agrega a esta suite con la misma verificación.

## 7. Resultados de referencia (DESKTOP-R3PLOD9)

Intel Core i5-11400F (6 núcleos, 12 hilos, L3 12 MB), 16 GB, Windows 10,
gcc 16.2 (WinLibs), `-O2`, mediana de 5. Datos completos y dispersión en
[`docs/HITO_H1.md`](docs/HITO_H1.md).

| n | naive | ikj | bloques 64 |
|---:|---:|---:|---:|
| 256 | 0.015 s | 0.008 s | 0.008 s |
| 512 | 0.166 s | 0.049 s | 0.055 s |
| 1024 | 1.487 s | 0.474 s | 0.445 s |
| 2048 | — | 4.173 s | 3.538 s |

Solo cambiar el orden de los bucles (naive → ikj) da **~3×** en n = 1024
(3.1–3.5× según la corrida), sin ningún paralelismo (en una VM de 1 núcleo
medimos 8×): es efecto de la caché.
Por eso el **speedup se calcula contra la mejor versión secuencial**.

En el simulador, K·U es el **~47 %** del tiempo (edificio de 100 × 400, 40 100
GDL, 1.9 ms por paso con 8 escenarios). En S7 hay que paralelizar también la
actualización de U: con solo K·U en paralelo, Amdahl limita el speedup a ~1.9×.

## 8. Flujo de trabajo con Git

```bash
git checkout -b s6-generadores     # una rama por tarea
# ... cambios ...
make test
git commit -m "Agrega generador de matrices de rigidez"
git push -u origin s6-generadores  # y se abre un Pull Request
```

Otro integrante revisa el Pull Request antes de unirlo a `main`.

## 9. Checklist de la Fase 1

**Semana 5**
- [ ] Repositorio en GitHub con todos los integrantes como colaboradores
- [ ] Cada integrante corrió `make env` y subió su `results/entorno_<nombre>.txt` (hay uno: `entorno.txt` de DESKTOP-R3PLOD9)
- [x] Ficha de lectura de cada artículo (`docs/fichas/`, en borrador: completar tras la lectura)
- [x] Tamaños de prueba y protocolo (`docs/PROTOCOLO_MEDICION.md`) — falta que el equipo los apruebe en acta
- [x] Ficha del modelo (`docs/MODELO_FISICO.md`)
- [ ] Al menos un acelerograma peruano real en `data/` con su fuente

**Semana 6**
- [x] `make test` pasa (70 pruebas) — repetir en la máquina de cada integrante
- [x] `make demo` corre con escenarios sintéticos — falta probarlo con un registro real
- [x] `make bench` corrido (DESKTOP-R3PLOD9) — repetir en la máquina oficial
- [x] Tabla de T(1) con mediana por tamaño (`docs/HITO_H1.md`)
- [x] Gráficos de respuesta y de T(1) (`results/figuras/`)
- [x] **Hito H1:** línea base y simulador validados ✔
