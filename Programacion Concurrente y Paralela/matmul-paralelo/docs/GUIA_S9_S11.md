# Guía de las semanas 9 a 11: MPI (memoria distribuida)

Paso a paso para **instalar MPI, compilar, probar, medir y mostrar** lo de las
semanas 9, 10 y 11. El diseño (qué guarda cada proceso y por qué) está en
[`DISENO_REPARTO_MPI.md`](DISENO_REPARTO_MPI.md), que es la tarea de S8.

| Semana | Entregable del cronograma | Dónde está |
|---|---|---|
| S9 | Cannon funcional y verificado (1, 4, 9 y 16 procesos) | `src/mpi/cannon.c`, grupo 3 de `tests/test_mpi.c` |
| S10 | Tabla comparativa del caso denso (SUMMA contra Cannon, bytes y mensajes) | `src/mpi/summa.c`, `bench/bench_mpi.c`, sección 6.1 |
| S10 | Reparto de los nodos de la estructura entre procesos | `partition_rows` en `src/mpi/sim_mpi.c` |
| S11 | **H3:** la simulación con MPI da los mismos desplazamientos que la secuencial con 1, 4, 9 y 16 procesos | `sim_run_mpi`, grupo 7 de `tests/test_mpi.c`, `make demo-mpi` |

---

## 0. Qué hay nuevo

| Archivo | Qué es |
|---|---|
| `include/par_mpi.h` | La API MPI y sus convenciones: todo es colectivo, los datos entran y salen por el proceso 0 |
| `src/mpi/cannon.c` | Cannon: malla `q × q` periódica, sesgo inicial y `q` rotaciones con `MPI_Sendrecv_replace` |
| `src/mpi/summa.c` | SUMMA: malla `pr × pc` (cualquier `p`), difusión de paneles con `MPI_Bcast` por fila y por columna |
| `src/mpi/sim_mpi.c` | Reparto en franjas de pisos (por filas o por nnz), nodos fantasma, intercambio de fronteras, `spmm_csr_mpi` y `sim_run_mpi` |
| `src/mpi/mpi_common.c` | Mallas, reparto y recolección de bloques, producto local, contadores de mensajes y bytes |
| `tests/test_mpi.c` | 29 a 34 pruebas según `p` (algunas solo aplican a `p` cuadrado o a `p > 1`) |
| `bench/bench_mpi.c` | Tiempo, comunicación, mensajes y bytes de Cannon y SUMMA |
| `bench/bench_sim_mpi.c` | S(p) del simulador repartido, por filas y por nnz |
| `apps/demo_sismo.c` | Compilado con `-DUSE_MPI` es `demo_sismo_mpi`: la demo con el edificio repartido |
| `scripts/graficos_s11.py` | Figuras 10, 11 y 12 y las tablas de esta guía |
| `apps/carrera_mpi.c` | La carrera: el mismo edificio con 1 a p procesos, tiempo, speedup y resultado idéntico |
| `DEMO_MPI.bat` | La demo en vivo del H3 (doble clic) |

Comandos nuevos del `Makefile` (en Windows, `mingw32-make`):

```
make mpi           compila las 4 versiones MPI
make test-mpi      pruebas con 1, 2, 4, 6, 9 y 16 procesos (unos 10 s)
make bench-mpi     Cannon, SUMMA y el simulador con 1, 2, 4, 8, 9 y 16 procesos (unos 15 min)
make demo-mpi      la demo del edificio con 4 procesos
make carrera-mpi   el edificio grande con 1, 2, 4 y 9 procesos: tiempo, speedup y resultado idéntico
make figuras-s11   figuras 10, 11 y 12 en results/figuras/
```

---

## 1. Instalar MPI

**Windows (lo que se usó en DESKTOP-62M3KIP):** MS-MPI, la implementación de
Microsoft. Pide permiso de administrador.

```powershell
winget install -e --id Microsoft.msmpi      # runtime: mpiexec.exe
winget install -e --id Microsoft.msmpisdk   # SDK: mpi.h y msmpi.lib
```

Queda en `C:\Program Files\Microsoft MPI\Bin\mpiexec.exe` (runtime) y
`C:\Program Files (x86)\Microsoft SDKs\MPI\` (SDK). El `Makefile` ya conoce
esas rutas. Cerrar y abrir Git Bash después de instalar.

**Linux o WSL2:** `sudo apt install openmpi-bin libopenmpi-dev`.

**¿Funciona igual que OpenMPI?** Para nuestro código, sí: los dos implementan
el estándar MPI, y las funciones que usamos (`MPI_Cart_create`,
`MPI_Cart_sub`, `MPI_Sendrecv_replace`, `MPI_Bcast`, `MPI_Isend`...) son
idénticas. Solo cambia lo de alrededor:

| | OpenMPI (Linux/WSL) | MS-MPI (Windows) |
|---|---|---|
| Compilar | `mpicc` | `gcc` + `-I<SDK>/Include` + `-lmsmpi` (lo hace el `Makefile`) |
| Ejecutar | `mpirun -np 4` o `mpiexec -n 4` | `mpiexec -n 4` |
| Más procesos que núcleos | necesita `--oversubscribe` | lo permite directamente (ojo: sección 7) |

---

## 2. Compilar

```bash
make mpi
```

Debe compilar **sin ninguna advertencia** `build/test_mpi.exe`,
`build/bench_mpi.exe`, `build/bench_sim_mpi.exe` y `build/demo_sismo_mpi.exe`,
con las mismas banderas que el secuencial (`-O2 -std=c11`). Antes de seguir,
confirmar que lo anterior sigue pasando: `make test` (70) y `make test-omp` (51).

---

## 3. Pruebas (criterio del Hito H3)

```bash
make test-mpi
```

Debe terminar en:

```
=== Resultado (p = 1): 33 pruebas OK, 0 fallas ===
=== Resultado (p = 2): 29 pruebas OK, 0 fallas ===
=== Resultado (p = 4): 34 pruebas OK, 0 fallas ===
=== Resultado (p = 6): 29 pruebas OK, 0 fallas ===
=== Resultado (p = 9): 34 pruebas OK, 0 fallas ===
=== Resultado (p = 16): 34 pruebas OK, 0 fallas ===
```

El detalle de cada corrida queda en `build/test_mpi_p<N>.txt`. Para ver una a
mano: `mpiexec -n 9 ./build/test_mpi`.

| Grupo | Qué prueba | Criterio |
|---|---|---|
| 1. Procesos y malla | Existen los `p` procesos; malla de Cannon (`q × q`) y de SUMMA (`pr × pc`) | — |
| 2. Ejemplo a mano | `[2x3]·[3x2] = [[58,64],[139,154]]` incluso con 16 procesos (casi todo es relleno) | exacto |
| 3. Cannon | 5 casos contra `matmul_naive`: 64², 37² (no múltiplo de q), 50×30·30×70, 1×1, 100×3·3×5 | ε = 1e-9 |
| 4. SUMMA | Los mismos 5 casos, con cualquier `p` (también 2 y 6) | ε = 1e-9 |
| 5. Reparto | Rangos contiguos que cubren todas las filas; repartir por nnz baja el desbalance (de 3.5 a 1.0 en filas desparejas) | — |
| 6. K·U distribuido | 3 matrices (rigidez 2D, aleatoria con vecinos en cualquier lado, edificio) × 2 repartos | **bit a bit** |
| 7. Simulador | Edificio 12×5 con 4 sismos, vibración libre con u0 y v0, y un edificio de 3×2 con más procesos que pisos: u final, picos, derivas e historia | **bit a bit** |
| 8. Errores | Dimensiones que no calzan; Cannon con `p` no cuadrado devuelve -1 en todos | — |

**¿Por qué Cannon se compara con ε y el simulador bit a bit?** En Cannon el
bloque `C(i,j)` empieza a acumular por el bloque `(i+j) mod q` del índice
interno: el orden de las sumas cambia y pueden cambiar los últimos bits (el
error real es ~1e-15). En el simulador cada fila de `K·U` la calcula un solo
proceso, con sus no nulos en el mismo orden que `spmm_csr`: no hay ninguna
razón para que cambie un bit, y si cambiara sería un error de reparto o un
fantasma desactualizado.

Comprobación extra con archivos (lo que se muestra en la demo):

```bash
make demo && cp results/respuesta.csv results/respuesta_seq.csv
mpiexec -n 16 ./build/demo_sismo_mpi 10 4 > /dev/null
cmp results/respuesta.csv results/respuesta_seq.csv && echo identicos
```

Se probó con 4, 9 y 16 procesos: `respuesta.csv`, `mapa_deriva.csv` e
`historia_techo.csv` son idénticos byte a byte a los del secuencial.

---

## 4. La demo (lo que se muestra en el H3)

```bash
make demo-mpi                                     # 10 pisos x 4 vanos, 4 procesos
mpiexec -n 9 ./build/demo_sismo_mpi 10 4
REPARTO=filas mpiexec -n 4 ./build/demo_sismo_mpi 10 4
mpiexec -n 8 ./build/demo_sismo_mpi 100 400       # el edificio grande
```

Sale lo mismo que `make demo` más una tabla con **la franja de pisos de cada
proceso**:

```
Version MPI: 4 procesos, reparto por no nulos (cambiar con REPARTO=filas o REPARTO=nnz)
  proceso  nodos          pisos           nnz  fantasmas  vecinos
  0        0..12          1..3             55          5        1
  1        13..24         3..5             55         10        2
  2        25..36         6..8             55         10        2
  3        37..49         8..10            55          5        1
  Desbalance de nnz: 1.00 (1.00 = perfecto). En cada paso viajan 6 mensajes de frontera.
```

Cómo leerla: el proceso 1 tiene los nodos 13 a 24 (parte del piso 3, el 4 y
parte del 5). Necesita 10 nodos de otros (5 de abajo y 5 de arriba: una fila
de nodos de cada vecino). Los procesos de los extremos tienen un solo vecino.
Con `REPARTO=filas` el proceso 0 se queda con 12 nodos y 50 nnz y el 1 con
13 nodos y 60 nnz (desbalance 1.09): el piso 1 tiene menos no nulos porque
debajo está el suelo.

### La carrera: que se vea la diferencia

```bash
make carrera-mpi            # o, a mano: mpiexec -n 1 ... y luego -n 2, 4, 9
mpiexec -n 1 ./build/carrera_mpi 100 400 20     # siempre primero con 1 proceso
mpiexec -n 9 ./build/carrera_mpi 100 400 20
```

El mismo edificio de 100 × 400 (40 100 nodos), con 8 sismos y 20 s de sismo
(3 286 pasos). Con 1 proceso corre el simulador **secuencial** y guarda su
tiempo y una huella del resultado en `build/carrera_ref.txt`. Con más
procesos muestra el tiempo, cuántas veces más rápido va y si el resultado es
idéntico bit a bit:

```
  quien calcula          tiempo   (barra mas corta = mas rapido)
  1 proceso (secuenc.)    4.73 s   ############################
  2 procesos MPI          2.70 s   ################                1.8 veces mas rapido | identico: SI
  4 procesos MPI          2.22 s   #############                   2.1 veces mas rapido | identico: SI
  9 procesos MPI          1.38 s   ########                        3.4 veces mas rapido | identico: SI
```

Con 9 procesos va entre 3.2 y 3.7 veces más rápido según la corrida (la laptop
mueve los procesos entre núcleos rápidos y eficientes). El tiempo incluye
repartir el edificio y juntar el resultado.

Para la demo completa en vivo, paso a paso: **`DEMO_MPI.bat`** (doble clic).
El paso 5 es la carrera.

---

## 5. Medir

### 5.1 Antes de medir

Lo mismo que en S7 (`docs/PROTOCOLO_MEDICION.md`): PC enchufada, nada pesado
abierto, mediana de 5. Las pruebas pasan antes de medir y cada programa
**vuelve a verificar** el resultado antes de cronometrar: si no coincide, se
detiene.

### 5.2 Correr

```bash
make bench-mpi                         # 1, 2, 4, 8, 9 y 16 procesos (unos 15 min)
make bench-mpi PROCS="1 4 9 16"        # solo los del criterio H3
make bench-mpi TAMANOS=512,1024        # sin n = 2048 (más rápido)
make figuras-s11
```

Guarda `results/mpi_<máquina>.csv` (Cannon y SUMMA) y
`results/mpi_sim_<máquina>.csv` (simulador) y, en pantalla, un resumen por
cada `p`. `make figuras-s11` hace las figuras e imprime las tablas en formato
Markdown listas para pegar.

**Con más procesos que núcleos lógicos** (16 en una laptop de 14 hilos), medir
también con espera pasiva (sección 7):

```bash
mkdir -p results/espera_pasiva
mpiexec -n 16 -env MSMPI_PROGRESS_SPIN_LIMIT 1 ./build/bench_mpi 5 512,1024,2048 > results/espera_pasiva/mpi_$(hostname).csv
mpiexec -n 16 -env MSMPI_PROGRESS_SPIN_LIMIT 1 ./build/bench_sim_mpi 5 400 8 > results/espera_pasiva/mpi_sim_$(hostname).csv
```

`graficos_s11.py` dibuja esas mediciones como estrellas en p = 16.

### 5.3 Qué es T(1)

Igual que en S7: **lo más rápido que se midió con un solo proceso**. En el
caso denso es `seq_ikj` o `seq_blocked64`. En el simulador, en esta laptop
`sim_mpi_nnz` con `p = 1` (0.627 s) salió más rápido que los dos
secuenciales (0.703 y 0.672 s), así que se usa ese: si no, el speedup quedaría
inflado por una diferencia que no viene del paralelismo. Es el mismo criterio
conservador de S7 (`spmm_csr_rowinit`).

---

## 6. Resultados de referencia (DESKTOP-62M3KIP)

Laptop: Intel Core Ultra 5 235U (12 núcleos: 2 de rendimiento + 8 eficientes
+ 2 de bajo consumo; 14 hilos), 15.4 GB, Windows 11, gcc 16.2 `-O2`, MS-MPI,
mediana de 5, 2026-11-05. **No es la máquina de S7** (DESKTOP-R3PLOD9): para el
informe final hay que repetir todo en la máquina oficial.

### 6.1 Caso denso: tabla comparativa Cannon contra SUMMA (entregable de S10)

`n = 1024`, T(1) = 0.298 s (`seq_ikj`). Tiempo de la llamada completa
(repartir A y B, calcular, juntar C). Mensajes y MB: recibidos durante el
cálculo, sumando todos los procesos.

| p | malla | Cannon [s] | S(p) | SUMMA [s] | S(p) | Cannon msgs / MB | SUMMA msgs / MB |
|---:|---|---:|---:|---:|---:|---:|---:|
| 2 | 1×2 | — | — | 0.157 | 1.90 | — | 2 / 8.4 |
| 4 | 2×2 | 0.115 | 2.58 | 0.113 | 2.63 | 12 / 25.2 | 8 / 16.8 |
| 8 | 2×4 | — | — | 0.084 | 3.57 | — | 40 / 33.6 |
| 9 | 3×3 | 0.084 | 3.56 | 0.084 | 3.55 | 48 / 44.9 | 36 / 33.7 |
| 16 | 4×4 | 0.128 | 2.32 | 0.145 | 2.06 | 120 / 62.9 | 96 / 50.3 |
| 16 (espera pasiva) | 4×4 | 0.075 | **3.96** | 0.075 | **3.97** | 120 / 62.9 | 96 / 50.3 |

`n = 2048`, T(1) = 2.89 s (`seq_blocked64`): Cannon 1.59× (p = 4), 2.24×
(p = 9) y 3.32× (p = 16); SUMMA 3.89× (p = 8) y 3.29× (p = 16). Con 2048
cada matriz ocupa 32 MB y el cálculo local queda limitado por la memoria RAM.

**Lo que dice la tabla:**

- **Cannon y SUMMA tardan casi lo mismo.** Con malla cuadrada hacen los
  mismos `q` pasos con bloques del mismo tamaño.
- **Cannon recibe un 25–50 % más de datos.** No por las rotaciones (son tantas
  como las difusiones de SUMMA) sino por el **sesgo inicial**: con `q = 4` son
  24 mensajes extra de 2 MB cada uno (bloques de 512 × 512) en `n = 2048`. Mensajes medidos
  = fórmula: Cannon `2·p·(q−1) + sesgo` = 96 + 24 = 120; SUMMA `2·p·(q−1)` = 96.
- **SUMMA corre con 2 y 8 procesos**, Cannon no (exige `p` cuadrado). Con
  8 procesos SUMMA es el más rápido de la tabla.
- **Repartir A y B y juntar C** (la franja amarilla de la figura 10) cuesta
  ~0.02 s en `n = 1024`: no baja con `p`, porque la raíz manda todo.

### 6.2 Simulador distribuido (H3)

400 pasos, 8 sismos, reparto por nnz. T(1) = lo más rápido con un proceso.

| Edificio | T(1) | S(2) | S(4) | S(8) | S(9) | S(16) | S(16) espera pasiva | % comunicación (p = 8) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 20 × 50 (1 020 nodos) | 0.017 s | 1.29 | 2.19 | 3.50 | 3.38 | 0.03 | 1.64 | 51 % |
| 50 × 200 (10 050 nodos) | 0.165 s | 1.23 | 2.04 | 3.64 | **3.96** | 0.26 | 3.04 | 46 % |
| 100 × 400 (40 100 nodos) | 0.627 s | 1.21 | 2.32 | 3.57 | 3.71 | 0.64 | 2.88 | 28 % |

Reparto por filas en 100 × 400: 3.74× con 8 y 3.77× con 9 (igual que por nnz
dentro del ruido: en este edificio el desbalance por filas es solo 1.004).

Comparación con OpenMP **en la misma laptop** (medido el 15/10,
`results/omp_sim_DESKTOP-62M3KIP.csv`): `sim_omp` con 8 hilos en 100 × 400
da 2.06×; MPI con 8 procesos, 3.57×.

---

## 7. Cómo interpretar los resultados

| Lo que vas a ver | Por qué |
|---|---|
| El speedup se queda en ~3.5–4× con 8–9 procesos | Esta laptop tiene solo **2 núcleos de rendimiento**; los otros 10 son más lentos. Con 9 procesos casi todos corren en núcleos eficientes, y todos comparten la misma memoria RAM |
| Con 16 procesos el simulador se desploma (0.03× a 0.64×) | **Más procesos (16) que hilos de hardware (14).** MS-MPI espera los mensajes con *espera activa* (un bucle que pregunta "¿ya llegó?" sin soltar el núcleo). Cuando un proceso espera a un vecino que no tiene núcleo, le roba la CPU justo a quien debe mandarle el dato. Como el simulador se sincroniza con sus vecinos 400 veces, el efecto se acumula |
| Con `MSMPI_PROGRESS_SPIN_LIMIT=1` (espera pasiva) 16 procesos vuelven a ~3× | El proceso que espera suelta el núcleo. Con 8 procesos, en cambio, la espera pasiva es un poco **más lenta** (despertar cuesta tiempo): solo conviene cuando hay más procesos que núcleos |
| El producto denso con 16 sufre menos que el simulador | Se sincroniza solo `q = 4` veces (no 400) y cada paso tiene mucho cálculo |
| El edificio grande escala mejor con muchos procesos | La comunicación por paso es siempre 2 filas de nodos; el cálculo baja con `p`. Razón comunicación/cálculo ∝ `p / pisos` (28 % de comunicación en 100 × 400 contra 51 % en 20 × 50, con 8 procesos) |
| MPI con 8 procesos (3.6×) le gana a OpenMP con 8 hilos (2.1×) en la misma laptop | En OpenMP todos los hilos se esperan en una **barrera** dos veces por paso; en MPI cada proceso espera solo a sus 2 vecinos. Además, cada proceso trabaja en su propia memoria. (Son mediciones de días distintos: confirmarlo en la máquina oficial) |
| Repartir por nnz o por filas da lo mismo en el edificio | Casi todas las filas tienen 5 no nulos (desbalance por filas: 1.004). La diferencia se ve en matrices desparejas (prueba del grupo 5: 3.5 contra 1.0) |
| Tiempos que saltan entre corridas (p. ej. 0.17 contra 0.21 s) | Windows mueve los procesos entre núcleos rápidos y lentos. Por eso la mediana de 5 |

Preguntas probables del docente:

1. **¿Por qué una malla 2D para el denso y franjas 1D para el edificio?** En el
   denso cada proceso necesita una fila de bloques de A y una columna de B: en
   2D eso es `2n²/√p` datos en vez de `n²`. En el edificio K es dispersa y
   cada nodo solo depende de sus vecinos: en franjas, cada proceso necesita
   solo 2 filas de nodos por paso.
2. **¿Qué es un nodo fantasma?** La copia local de un nodo de otro proceso que
   necesito para mi `K·U` (o para la deriva). Se actualiza en cada paso con el
   intercambio de fronteras.
3. **¿Por qué `MPI_Sendrecv_replace` en Cannon?** Manda y recibe en la misma
   llamada y sobre el mismo búfer: todos rotan a la vez sin interbloqueo (si
   todos hicieran `MPI_Send` primero, podrían quedarse esperando) y sin memoria
   extra.
4. **¿Por qué `MPI_Isend`/`MPI_Irecv` en el simulador?** Un proceso tiene 2
   vecinos: con llamadas no bloqueantes se lanzan los 4 mensajes a la vez y
   se espera al final (`MPI_Waitall`), sin depender del orden.
5. **¿Por qué SUMMA si ya está Cannon?** Funciona con cualquier `p` (2, 6, 8...),
   no necesita el sesgo inicial y es el algoritmo de bibliotecas como
   ScaLAPACK. Cannon necesita `p = q²` y una malla periódica.
6. **¿El resultado del simulador es aproximado?** No: idéntico bit a bit al
   secuencial con cualquier `p` (sección 3).

---

## 8. Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `mpi.h: No such file or directory` | Falta el SDK: `winget install -e --id Microsoft.msmpisdk` |
| `mpiexec: command not found` | Falta el runtime, o la terminal se abrió antes de instalarlo: cerrar y abrir Git Bash. El `Makefile` usa la ruta completa, así que `make test-mpi` funciona igual |
| Se queda colgado al correr | Algún proceso falló antes de una función colectiva. Ctrl+C; correr `mpiexec -n 1` para ver el mensaje de error |
| Con 16 procesos todo es muy lento | Sección 7: más procesos que hilos. Usar `-env MSMPI_PROGRESS_SPIN_LIMIT 1` o no pasar de 14 |
| `test_mpi`: falla el grupo 7 | Un fantasma desactualizado o un índice local mal calculado: comparar `build/test_mpi_p<N>.txt` entre `p` |
| Linux: `There are not enough slots available` | OpenMPI con más procesos que núcleos: `mpirun --oversubscribe -n 16` (el `Makefile` ya lo pone) |

---

## 9. Checklist de las semanas 8 a 11

**Semana 8**
- [x] Simulación en paralelo de un edificio pequeño con OpenMP (`make demo-omp`, de S7)
- [x] Diseño del reparto para MPI (`docs/DISENO_REPARTO_MPI.md`)
- [x] Informe de avance (`Exposicion/Informe_avance_S8_S11.pdf`)

**Semana 9**
- [x] Malla 2D de procesos con comunicadores de fila y columna (`MPI_Cart_create`, `MPI_Cart_sub`)
- [x] Cannon con desplazamientos circulares
- [x] Mismo resultado con 1, 4, 9 y 16 procesos

**Semana 10**
- [x] SUMMA con `MPI_Bcast` por filas y columnas
- [x] Bytes y número de mensajes medidos; comparación con Cannon (sección 6.1)
- [x] Reparto de los nodos de la estructura entre procesos (`partition_rows`)

**Semana 11**
- [x] Cada proceso avanza en el tiempo su parte de la estructura
- [x] Intercambio de nodos de frontera en cada paso
- [x] Balanceo de carga según nnz por proceso
- [x] **Hito H3** cumplido en DESKTOP-62M3KIP: `make test-mpi` y los CSV de la demo idénticos con 4, 9 y 16 procesos

**Pendiente (no depende del código)**
- [ ] `make test-mpi` y `make bench-mpi` en la **máquina oficial** y en la de cada integrante
- [ ] Anotar en `docs/actas.md` la decisión de MS-MPI en Windows y la de la espera pasiva para `p` > núcleos
