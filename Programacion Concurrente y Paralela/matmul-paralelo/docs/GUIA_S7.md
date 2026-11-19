# Guía de la semana 7 — OpenMP y Go (memoria compartida)

Paso a paso para **compilar, probar y medir** lo de la semana 7. El código ya
está escrito; lo que falta es **correrlo en tu máquina**, revisar que todo
salga bien, guardar los CSV y llenar la tabla de speedup.

**Entregable de S7:** S(p) y E(p) con 1, 2, 4 y 8 hilos.
**Hito H2 (S8):** OpenMP y Go coinciden con el secuencial con 1 a 8 hilos, y
se muestra la respuesta de un edificio pequeño ante un sismo.

---

## 0. Qué hay nuevo

| Archivo | Qué es |
|---|---|
| `include/par_omp.h` | Las 4 funciones OpenMP y por qué no tienen condiciones de carrera |
| `src/omp/matmul_omp.c` | `matmul_omp_ikj`, `matmul_omp_blocked` (por bloques) y `spmm_csr_omp` (K·U) |
| `src/omp/sim_omp.c` | `sim_run_omp`: el simulador con **todo** el paso en paralelo (no solo K·U) |
| `src/seq.c` → `spmm_csr_rowinit` | Variante secuencial nueva de K·U: es la T(1) justa del caso disperso (sección 5.3) |
| `tests/test_omp.c` | 51 pruebas: OpenMP **idéntico bit a bit** al secuencial con 1, 2, 3, 4 y 8 hilos |
| `bench/bench_omp.c` | S(p) y E(p) del producto denso y disperso |
| `bench/bench_sim_omp.c` | S(p) y E(p) del simulador: "solo K·U" vs. "todo el paso" (Amdahl) |
| `apps/demo_sismo.c` | Si se compila con `-fopenmp` usa el simulador OpenMP (`make demo-omp`) |
| `go/` | Versión Go: *worker pool* de goroutines (`paralelo.go`), pruebas y medición |
| `scripts/graficos_s7.py` | Gráficos de speedup (figuras 08 y 09) |

Comandos nuevos del `Makefile` (en Windows, `mingw32-make` en lugar de `make`):

```
make omp         compila las versiones OpenMP
make test-omp    pruebas OpenMP (51)
make bench-omp   mide S(p) y E(p) del producto y del simulador (unos 5 min)
make demo-omp    la demo del edificio con el simulador OpenMP
make go-test     pruebas de Go
make go-bench    mide S(p) y E(p) de Go (unos 2 min)
make figuras-s7  gráficos de speedup en results/figuras/
```

Los comandos de S5–S6 (`make test`, `make demo`, `make bench`...) siguen igual.

---

## 1. Preparar la terminal (una vez por sesión)

Abrir **Git Bash** en la carpeta `matmul-paralelo` y comprobar:

```bash
gcc --version          # MinGW-W64 ... 16.2 (WinLibs)
mingw32-make --version
go version             # go1.21 o más nuevo
```

Si `gcc` o `mingw32-make` dicen *command not found*, el PATH de WinLibs no
está en esa terminal. Agregarlo así (o cerrar y abrir Git Bash):

```bash
export PATH="$LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH"
alias make=mingw32-make     # opcional: para escribir "make"
```

En Linux o WSL todo es igual pero con `make`.

---

## 2. Compilar

```bash
make omp
```

Debe compilar **sin ninguna advertencia** 4 programas: `build/test_omp.exe`,
`build/bench_omp.exe`, `build/bench_sim_omp.exe` y `build/demo_sismo_omp.exe`.
Todos usan las mismas banderas que el secuencial (`-O2 -std=c11`) más
`-fopenmp`. El secuencial de referencia va **dentro** de cada programa, así que
la comparación es justa.

Antes de seguir, confirmar que lo de la Fase 1 sigue pasando:

```bash
make test        # 47 + 23 pruebas OK (no debe haber cambiado nada)
```

---

## 3. Pruebas de OpenMP

```bash
make test-omp
```

Debe terminar en:

```
=== Resultado: 51 pruebas OK, 0 fallas ===
```

| Grupo | Qué prueba | Pruebas |
|---|---|---|
| 1. Los hilos existen | Cuenta los hilos dentro de una región paralela: deben ser p | 5 |
| 2. Ejemplo a mano | `[2x3]·[3x2] = [[58,64],[139,154]]` con **8 hilos y solo 2 filas** | 2 |
| 3. Producto denso | `omp_ikj` y `omp_blocked` (bloques 7, 32, 64) contra naive, matrices impares y rectangulares | 20 |
| 4. Producto disperso | `spmm_csr_omp` en 3 matrices distintas; y la variante `spmm_csr_rowinit` | 6 |
| 5. Simulador | Solo K·U en paralelo, simulador completo (u, picos, derivas, historia) y vibración libre | 15 |
| 6. Errores | Dimensiones que no calzan devuelven -1 | 3 |

Cada grupo se repite con **p = 1, 2, 3, 4 y 8**. El 3 está a propósito: casi
ningún tamaño es múltiplo de 3, así que prueba que el reparto disparejo
también funciona.

**¿Por qué "idéntico bit a bit" y no "dentro de ε"?** En nuestras versiones
cada hilo calcula filas completas, así que cada elemento se suma en el mismo
orden que en el secuencial. No hay ninguna razón para que cambie ni un bit.
Si una prueba falla, es un error de reparto o una condición de carrera, no un
redondeo.

Para ver que funciona con cualquier número de hilos, también se puede probar a
mano la demo (sección 6) con `OMP_NUM_THREADS=1`, `3`, `8` o `12`.

---

## 4. Pruebas de Go

```bash
cd go
go test -v
cd ..
```

(o `make go-test` desde la raíz). Debe terminar en `PASS` y `ok matmulgo`.

| Prueba | Qué comprueba |
|---|---|
| `TestEjemploAMano` | `[[58,64],[139,154]]`, también con 8 trabajadores para 2 filas |
| `TestGeneradorIgualAC` | Go genera **las mismas matrices que C** (mismo splitmix64) y el producto 64×64 da exactamente lo mismo que en C |
| `TestPoolIgualSecuencial` | El pool con 1, 2, 3, 4 y 8 trabajadores y tareas de 1, 7 y 64 filas: idéntico al secuencial (15 subpruebas) |
| `TestSpmm` | K·U contra el producto denso, K·1 = 0 en un nodo interior, y el pool idéntico al secuencial |
| `TestErrores` | Dimensiones que no calzan devuelven `ErrDimensiones` |

Opcionales:

```bash
cd go
go test -race          # detector de condiciones de carrera de Go
go test -bench . -run '^$'   # tiempo del pool con 1, 2, 4 y 8 trabajadores (n = 512)
```

`go test -race` necesita cgo: en Windows, `gcc` en el PATH (el de WinLibs
sirve) y `CGO_ENABLED=1`. Si dice *-race requires cgo*, correr
`CGO_ENABLED=1 go test -race`. Debe decir `ok` sin ningún `WARNING: DATA RACE`.

---

## 5. Medir S(p) y E(p)

### 5.1 Antes de medir (docs/PROTOCOLO_MEDICION.md)

- PC conectada a la corriente, **sin** navegador, IDE indexando, videollamada,
  OBS ni nada pesado abierto.
- Siempre en la **misma máquina** para una misma tabla.
- `make test-omp` y `make go-test` pasan. Los programas de medición igual lo
  vuelven a comprobar: si el resultado no coincide con el secuencial, se
  detienen sin medir.

### 5.2 OpenMP

```bash
make bench-omp
```

Tarda unos 5 minutos. Guarda dos CSV:

- `results/omp_<máquina>.csv`: producto denso (n = 256, 512, 1024, 2048) y
  disperso (mallas de 128², 256² y 512² nodos con 16 escenarios).
- `results/omp_sim_<máquina>.csv`: simulador con edificios de 20×50, 50×200 y
  100×400 (400 pasos, 8 escenarios).

En pantalla sale el resumen con S(p) y E(p). **Copiarlo** (o guardar la salida
con `make bench-omp 2> results/omp_resumen.txt`). Ejemplo:

```
Denso n = 2048: T(1) = 3.5 s (mejor secuencial: blocked64)
  algoritmo        p   mediana_s   GFLOP/s    S(p)    E(p)
  omp_ikj          1      ...
  ...
```

Para medir otros hilos (por ejemplo los 6 núcleos físicos y los 12 lógicos):

```bash
make bench-omp HILOS=1,2,4,6,8,12
```

### 5.3 Ojo con T(1): la mejor secuencial, no "OpenMP con 1 hilo"

S(p) = T(1)/T(p), con **T(1) = la versión secuencial más rápida**, medida en
la misma corrida:

- **Denso:** la más rápida entre `ikj` y `blocked64`.
- **Disperso y simulador:** la más rápida entre `spmm_csr` y `spmm_csr_rowinit`.

**Por qué existe `spmm_csr_rowinit`.** En la primera medición, `spmm_csr_omp`
con **un solo hilo** salía ~1.3× más rápido que `spmm_csr`. Eso no es
paralelismo: la versión OpenMP pone cada fila en cero justo antes de usarla
(cuando ya está en caché), mientras que `spmm_csr` limpia toda la matriz antes
con `mat_zero`, en una pasada extra por memoria. Si se dejaba así, el speedup
salía inflado (eficiencias mayores que 1). Por eso se agregó la misma mejora
como versión secuencial (`spmm_csr_rowinit`, mismo resultado bit a bit) y se
usa la más rápida como T(1). Es el mismo criterio de la Fase 1: medir contra
la mejor secuencial para no atribuirle al paralelismo una mejora que no viene
de él.

### 5.4 Go

```bash
make go-bench
```

o, desde `go/`:

```bash
go run . -reps 5 -hilos 1,2,4,8 -n 256,512,1024 > ../results/go_$(hostname).csv
```

T(1) de Go es **la secuencial en Go** (`go_ikj`, `go_spmm`). Además del
speedup, comparar los **tiempos absolutos** de Go contra C: Go no vectoriza el
bucle interno, así que con el mismo algoritmo suele ser más lento. Es un dato
interesante para el informe.

### 5.5 Gráficos

```bash
make figuras-s7
```

Genera en `results/figuras/`:

- `08_speedup_productos.png`: S(p) del producto denso y de K·U, OpenMP y Go,
  con la recta ideal S(p) = p.
- `09_speedup_simulador.png`: el simulador "todo en paralelo" contra "solo K·U"
  y la curva de Amdahl; a la derecha, E(p) por tamaño de edificio.

### 5.6 Tabla para el informe (llenar con tus medianas)

| Caso | T(1) [s] | S(2) | S(4) | S(8) | E(8) |
|---|---:|---:|---:|---:|---:|
| Denso n = 2048, OpenMP bloques 64 | | | | | |
| Denso n = 2048, OpenMP ikj | | | | | |
| K·U malla 512² (n = 262 144), OpenMP | | | | | |
| Simulador 100×400, solo K·U en paralelo | | | | | |
| Simulador 100×400, todo el paso en paralelo | | | | | |
| Denso n = 1024, Go worker pool | | | | | |
| K·U malla 512², Go worker pool | | | | | |

Valores de referencia medidos en DESKTOP-R3PLOD9 (i5-11400F, 6 núcleos / 12
hilos): ver la sección 9. Si los tuyos salen muy distintos, revisar 5.1.

---

## 6. Demo con OpenMP (lo que se muestra en el H2)

```bash
make demo-omp                              # 10 pisos x 4 vanos, 4 hilos
OMP_NUM_THREADS=8 ./build/demo_sismo_omp 10 4
OMP_NUM_THREADS=8 ./build/demo_sismo_omp 30 20   # un edificio más grande
```

Sale lo mismo que en `make demo` (periodo, tablas por piso, resonancia a
1 Hz) más la línea `Version OpenMP: N hilos`. **Los resultados son idénticos**
a los del secuencial. Para comprobarlo con los archivos:

```bash
make demo
cp results/respuesta.csv results/respuesta_seq.csv
OMP_NUM_THREADS=8 ./build/demo_sismo_omp 10 4 > /dev/null
cmp results/respuesta.csv results/respuesta_seq.csv && echo "identicos"
```

En un edificio de 10×4 (50 nodos) la versión OpenMP **no** es más rápida:
hay muy poco trabajo por paso y crear y sincronizar hilos cuesta más que lo
que se gana. Se nota en edificios grandes (sección 7).

Con un registro real, igual que en la Fase 1:

```bash
OMP_NUM_THREADS=8 ./build/demo_sismo_omp 10 4 data/registro.txt 0.01 0.01 1
```

---

## 7. Cómo interpretar los resultados

| Lo que vas a ver | Por qué |
|---|---|
| En el denso, hasta 4 hilos S(p) crece casi como p | Hay trabajo de sobra y cada hilo trabaja en sus filas sin esperar a nadie |
| De 4 a 8 hilos, S(p) crece menos y E(p) baja | El i5-11400F tiene 6 núcleos físicos; con 8 hilos dos núcleos se comparten (*hyperthreading*). Y la memoria RAM es una sola para todos |
| K·U escala menos que el producto denso | K·U hace pocas cuentas por cada dato que lee (≤ 5 no nulos por fila): está limitado por la **memoria**, no por la CPU. El denso reutiliza cada dato n veces |
| "Solo K·U" se queda en ~1.3–1.4× | **Ley de Amdahl:** K·U es ~45–50 % del paso; el resto sigue en un solo hilo. El límite teórico con 8 hilos es ~1.7× |
| "Todo el paso" sí pasa ese límite | `sim_run_omp` reparte también la actualización de U y los picos |
| El edificio de 20×50 no gana nada (S < 1) | **Granularidad:** ~40 µs de trabajo por paso; abrir y cerrar regiones paralelas en cada paso cuesta más de lo que se reparte |
| Go con 1 trabajador ≈ Go secuencial | El pool con un solo trabajador hace lo mismo que el secuencial más el costo del canal |

Preguntas probables del docente:

1. **¿Por qué no hay condiciones de carrera?** Cada hilo (o goroutine) escribe
   solo sus filas de C. Dos hilos pueden **leer** la misma fila de U o de B,
   y eso no es problema; solo es peligroso que dos **escriban** lo mismo.
2. **¿Por qué `collapse(2)` en el producto por bloques?** Para que la lista de
   tareas sean todos los bloques de C (256 en n = 1024), no solo las 16
   filas de bloques. Con 16 tareas y 8 hilos, el reparto queda disparejo.
3. **¿Por qué `schedule(static)`?** Todas las filas cuestan lo mismo, así que
   repartirlas en trozos iguales es lo más barato. En Go el canal reparte de
   forma dinámica (el que termina antes toma la siguiente tarea).
4. **¿Qué hace la barrera en `sim_run_omp`?** La deriva de un nodo usa el
   desplazamiento del nodo de abajo, que puede calcularlo otro hilo. La
   barrera implícita al final del primer `omp for` garantiza que U(n+1) esté
   completo antes de calcular los picos.
5. **¿Por qué el resultado es idéntico en Go y en C?** Mismo generador
   (splitmix64), mismo orden de las sumas y `float64(a*b)` en Go para que el
   compilador no use FMA (en ARM lo haría y cambiaría el último bit).

---

## 8. Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `mingw32-make: command not found` | PATH de WinLibs: ver la sección 1 |
| `omp.h: No such file or directory` | El gcc no trae OpenMP. Con WinLibs sí viene; en macOS usar `brew install gcc` y `CC=gcc-14` |
| `test_omp`: grupo 1 falla (menos hilos que p) | Alguna variable `OMP_THREAD_LIMIT` o `OMP_DYNAMIC` en el entorno: `unset OMP_THREAD_LIMIT OMP_DYNAMIC` |
| Tiempos muy distintos entre repeticiones | Otro programa usando la CPU; cerrar todo y repetir. Por eso se usa la mediana |
| La primera corrida tarda mucho más | El antivirus revisa el `.exe` nuevo; es normal, la medición tiene calentamiento |
| `go: command not found` | Instalar Go (<https://go.dev/dl/>) y abrir otra terminal |
| `-race requires cgo` | `CGO_ENABLED=1 go test -race` con `gcc` en el PATH |
| `results/...csv: No such file or directory` | Correr desde la raíz `matmul-paralelo`, no desde `go/` ni `build/` |

---

## 9. Valores de referencia (DESKTOP-R3PLOD9)

Primera medición completa, 2026-10-14: Intel Core i5-11400F (6 núcleos, 12
hilos, L3 de 12 MB), 16 GB, Windows 10, gcc 16.2 `-O2 -fopenmp`, Go 1.27,
mediana de 5. Son los números de la exposición; **tu medición oficial los
reemplaza**. Si salen muy distintos, revisar la sección 5.1.

| Caso | T(1) | S(2) | S(4) | S(8) | E(8) |
|---|---:|---:|---:|---:|---:|
| Denso n = 1024, OpenMP bloques 64 | 0.445 s | 1.97 | 3.38 | 4.71 | 0.59 |
| Denso n = 2048, OpenMP bloques 64 | 3.58 s | 1.88 | 3.38 | 3.73 | 0.47 |
| Denso n = 2048, OpenMP ikj | 3.58 s | 1.48 | 2.56 | 3.15 | 0.39 |
| K·U malla 128² (n = 16 384), OpenMP | 0.66 ms | 1.72 | 2.22 | 3.66 | 0.46 |
| K·U malla 512² (n = 262 144), OpenMP | 9.86 ms | 1.78 | 1.89 | 2.38 | 0.30 |
| Simulador 100×400, solo K·U en paralelo | 0.751 s | 1.27 | 1.35 | 1.31 | 0.16 |
| ↳ límite de Amdahl con f = 46 % | | 1.30 | 1.54 | 1.69 | |
| Simulador 100×400, todo el paso en paralelo | 0.751 s | 1.66 | 2.36 | 2.54 | 0.32 |
| Simulador 50×200, todo el paso en paralelo | 0.165 s | 1.61 | 2.62 | 2.50 | 0.31 |
| Simulador 20×50, todo el paso en paralelo | 0.017 s | 0.83 | 0.87 | 0.59 | 0.07 |
| Denso n = 1024, Go worker pool | 0.514 s | 1.92 | 3.87 | 5.67 | 0.71 |
| K·U malla 512², Go worker pool | 14.0 ms | 1.89 | 3.05 | 3.20 | 0.40 |

Lo que dicen estos números:

- **El denso escala mejor que K·U.** El denso hace ~n operaciones por cada
  dato que trae de memoria; K·U, muy pocas. Con 4 hilos K·U ya está limitado
  por la memoria RAM (1.9× en la malla de 512²), aunque en una malla que cabe
  en la caché (128²) llega a 3.7×.
- **n = 2048 escala peor que n = 1024**: cada matriz ocupa 32 MB y ya no cabe
  en los 12 MB de L3.
- **Amdahl en vivo:** paralelizar solo K·U se queda en ~1.3×, por debajo de su
  límite teórico (1.7×). Paralelizar todo el paso llega a 2.5×.
- **Go escala más (5.7× contra 4.7×) pero parte más lento** (T(1) de 0.514 s
  contra 0.445 s en C). En tiempo absoluto con 8 hilos quedan casi empatados:
  0.091 s Go contra 0.095 s C en el denso de 1024, y 4.4 ms contra 4.1 ms en K·U.
- **Edificio chico (20×50): no conviene paralelizar.** Hay ~40 µs de trabajo
  por paso: menos que lo que cuesta coordinar a los hilos.

---

## 10. Checklist de la semana 7

- [ ] `make test` sigue en 70 pruebas OK
- [ ] `make test-omp`: 51 pruebas OK (en la máquina oficial y en la de cada integrante)
- [ ] `go test -v`: PASS (y `go test -race` sin advertencias, si se puede)
- [ ] `make bench-omp` y `make go-bench` en la máquina oficial; CSV en `results/`
- [ ] Tabla de S(p) y E(p) llenada (sección 5.6)
- [ ] `make figuras-s7`: figuras 08 y 09
- [ ] `make demo-omp` corre y da lo mismo que `make demo`
- [ ] Anotar en `docs/actas.md` la decisión de T(1) (`spmm_csr_rowinit`) y la máquina oficial
