# Proyecto: Multiplicación de Matrices Paralelas — UNMSM
# Fase 1 (Fundamentos y modelo): versión secuencial, simulador sísmico,
# verificación y línea base T(1).
#
#   make env         -> revisa gcc, OpenMP, MPI, CUDA, Go y Python; guarda results/entorno.txt
#   make test        -> verificación del producto de matrices y del simulador (70 pruebas)
#   make validar     -> oscilador de 1 GDL: curvas simulado vs. exacto y convergencia
#   make demo        -> edificio de 10 pisos ante 4 sismos sintéticos
#   make bench       -> mide T(1) del producto de matrices y del simulador (unos minutos)
#   make bench-sim   -> solo el simulador (rápido)
#   make figuras     -> gráficos en results/figuras/ (Python + matplotlib)
#   make entregable  -> todo lo anterior en orden: lo que se presenta en S6
#   make clean       -> borra build/
#
# Semana 7 — memoria compartida (instrucciones en docs/GUIA_S7.md):
#   make omp         -> compila las versiones OpenMP (pruebas, mediciones y demo)
#   make test-omp    -> OpenMP idéntico al secuencial con 1, 2, 3, 4 y 8 hilos
#   make bench-omp   -> S(p) y E(p) del producto de matrices y del simulador
#   make demo-omp    -> la demo del edificio con el simulador OpenMP (4 hilos)
#   make go-test     -> pruebas de la versión Go (worker pool de goroutines)
#   make go-bench    -> S(p) y E(p) de la versión Go
#   make figuras-s7  -> gráficos de speedup y eficiencia en results/figuras/
#
# Semanas 9 a 11 — memoria distribuida con MPI (instrucciones en docs/GUIA_S9_S11.md):
#   make mpi         -> compila las versiones MPI (Cannon, SUMMA, simulador repartido)
#   make test-mpi    -> pruebas MPI con 1, 2, 4, 6, 9 y 16 procesos (Hito H3)
#   make bench-mpi   -> tiempos y comunicación de Cannon, SUMMA y el simulador
#   make demo-mpi    -> la demo del edificio con la estructura repartida (4 procesos)
#   make carrera-mpi -> el mismo edificio grande con 1, 2, 4 y 9 procesos: tiempo y speedup
#   make figuras-s11 -> gráficos de speedup y comunicación en results/figuras/
#
# Linux / WSL: make ...      Windows (WinLibs, desde Git Bash): mingw32-make ...
#
# IMPORTANTE: todas las versiones (secuencial y paralelas) se compilan con
# las MISMAS banderas de optimización, o el speedup no es comparable.

CC      = gcc
CFLAGS  = -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
INC     = -Iinclude
LDLIBS  = -lm
BUILD   = build

ifeq ($(OS),Windows_NT)
  EXE = .exe
  PY ?= python
else
  EXE =
  PY ?= python3
endif

LIB_SRC = src/matrix.c src/generators.c src/seq.c src/verify.c src/timer.c \
          src/building.c src/ground_motion.c src/sim.c
HEADERS = $(wildcard include/*.h)

TEST_SEQ  = $(BUILD)/test_seq$(EXE)
TEST_SIM  = $(BUILD)/test_sim$(EXE)
BENCH_SEQ = $(BUILD)/bench_seq$(EXE)
BENCH_SIM = $(BUILD)/bench_sim$(EXE)
DEMO      = $(BUILD)/demo_sismo$(EXE)
VALIDAR   = $(BUILD)/validar_1gdl$(EXE)

# Semana 7: mismas CFLAGS + -fopenmp (el secuencial de referencia se compila
# dentro de los mismos programas, con las mismas banderas)
OMPFLAGS      = -fopenmp
OMP_SRC       = src/omp/matmul_omp.c src/omp/sim_omp.c
TEST_OMP      = $(BUILD)/test_omp$(EXE)
BENCH_OMP     = $(BUILD)/bench_omp$(EXE)
BENCH_SIM_OMP = $(BUILD)/bench_sim_omp$(EXE)
DEMO_OMP      = $(BUILD)/demo_sismo_omp$(EXE)
HILOS        ?= 1,2,4,8

# Semanas 9 a 11: MPI. Mismas CFLAGS (el secuencial de referencia va dentro).
# Windows: MS-MPI (gcc + cabecera y biblioteca del SDK de MS-MPI).
# Linux/WSL: OpenMPI o MPICH (mpicc). El código fuente es el mismo.
ifeq ($(OS),Windows_NT)
  MSMPI_INC   ?= C:/Program Files (x86)/Microsoft SDKs/MPI/Include/
  MSMPI_LIB64 ?= C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64/
  MSMPI_BIN   ?= C:/Program Files/Microsoft MPI/Bin/
  MPICC    = $(CC)
  MPIFLAGS = -I"$(MSMPI_INC)"
  MPILIBS  = -L"$(MSMPI_LIB64)" -lmsmpi
  MPIEXEC ?= "$(MSMPI_BIN)mpiexec.exe" -n
else
  MPICC    = mpicc
  MPIFLAGS =
  MPILIBS  =
  MPIEXEC ?= mpirun --oversubscribe -n     # MPICH: MPIEXEC="mpiexec -n"
endif
MPI_SRC       = src/mpi/mpi_common.c src/mpi/cannon.c src/mpi/summa.c src/mpi/sim_mpi.c
MPI_HEADERS   = $(HEADERS) src/mpi/mpi_internal.h
TEST_MPI      = $(BUILD)/test_mpi$(EXE)
BENCH_MPI     = $(BUILD)/bench_mpi$(EXE)
BENCH_SIM_MPI = $(BUILD)/bench_sim_mpi$(EXE)
DEMO_MPI      = $(BUILD)/demo_sismo_mpi$(EXE)
CARRERA_MPI   = $(BUILD)/carrera_mpi$(EXE)
MOTOR_MPI     = $(BUILD)/motor_mpi$(EXE)
PROCS_TEST   ?= 1 2 4 6 9 16
PROCS        ?= 1 2 4 8 9 16
TAMANOS      ?= 512,1024,2048

.PHONY: all test validar demo bench bench-sim figuras entregable env clean \
        omp test-omp bench-omp demo-omp go-test go-bench figuras-s7 \
        mpi test-mpi bench-mpi demo-mpi carrera-mpi figuras-s11

all: $(TEST_SEQ) $(TEST_SIM) $(BENCH_SEQ) $(BENCH_SIM) $(DEMO) $(VALIDAR)

$(BUILD):
	mkdir -p $(BUILD)

results:
	mkdir -p results

$(TEST_SEQ): tests/test_seq.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) tests/test_seq.c $(LIB_SRC) -o $@ $(LDLIBS)

$(TEST_SIM): tests/test_sim.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) tests/test_sim.c $(LIB_SRC) -o $@ $(LDLIBS)

$(BENCH_SEQ): bench/bench_seq.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) bench/bench_seq.c $(LIB_SRC) -o $@ $(LDLIBS)

$(BENCH_SIM): bench/bench_sim.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) bench/bench_sim.c $(LIB_SRC) -o $@ $(LDLIBS)

$(DEMO): apps/demo_sismo.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) apps/demo_sismo.c $(LIB_SRC) -o $@ $(LDLIBS)

$(VALIDAR): apps/validar_1gdl.c $(LIB_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) apps/validar_1gdl.c $(LIB_SRC) -o $@ $(LDLIBS)

test: $(TEST_SEQ) $(TEST_SIM)
	./$(TEST_SEQ)
	./$(TEST_SIM)

validar: $(VALIDAR) | results
	./$(VALIDAR)

demo: $(DEMO) | results
	./$(DEMO) 10 4 | tee results/demo_salida.txt

bench: $(TEST_SEQ) $(TEST_SIM) $(BENCH_SEQ) $(BENCH_SIM) | results
	./$(TEST_SEQ) > /dev/null && ./$(TEST_SIM) > /dev/null || (echo "La verificación falla: no se mide nada." && exit 1)
	./$(BENCH_SEQ) 5 256 512 1024 2048 > results/t1_$$(hostname).csv
	@echo "CSV guardado en results/t1_$$(hostname).csv"
	./$(BENCH_SIM) 5 400 8 > results/t1_sim_$$(hostname).csv
	@echo "CSV guardado en results/t1_sim_$$(hostname).csv"

bench-sim: $(TEST_SIM) $(BENCH_SIM) | results
	./$(TEST_SIM) > /dev/null || (echo "La verificación falla: no se mide nada." && exit 1)
	./$(BENCH_SIM) 5 400 8 > results/t1_sim_$$(hostname).csv
	@echo "CSV guardado en results/t1_sim_$$(hostname).csv"

figuras: | results
	$(PY) scripts/graficos.py

entregable: test validar demo bench figuras
	@echo
	@echo "Listo: resultados en results/ y figuras en results/figuras/"

env: | results
	bash scripts/check_env.sh

# ---------------- Semana 7: OpenMP y Go ----------------

omp: $(TEST_OMP) $(BENCH_OMP) $(BENCH_SIM_OMP) $(DEMO_OMP)

$(TEST_OMP): tests/test_omp.c $(LIB_SRC) $(OMP_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) tests/test_omp.c $(LIB_SRC) $(OMP_SRC) -o $@ $(LDLIBS)

$(BENCH_OMP): bench/bench_omp.c $(LIB_SRC) $(OMP_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) bench/bench_omp.c $(LIB_SRC) $(OMP_SRC) -o $@ $(LDLIBS)

$(BENCH_SIM_OMP): bench/bench_sim_omp.c $(LIB_SRC) $(OMP_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) bench/bench_sim_omp.c $(LIB_SRC) $(OMP_SRC) -o $@ $(LDLIBS)

$(DEMO_OMP): apps/demo_sismo.c $(LIB_SRC) $(OMP_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) apps/demo_sismo.c $(LIB_SRC) $(OMP_SRC) -o $@ $(LDLIBS)

test-omp: $(TEST_OMP)
	./$(TEST_OMP)

bench-omp: $(TEST_OMP) $(BENCH_OMP) $(BENCH_SIM_OMP) | results
	./$(TEST_OMP) > /dev/null || (echo "La verificación OpenMP falla: no se mide nada." && exit 1)
	./$(BENCH_OMP) 5 $(HILOS) 256 512 1024 2048 > results/omp_$$(hostname).csv
	@echo "CSV guardado en results/omp_$$(hostname).csv"
	./$(BENCH_SIM_OMP) 5 400 8 $(HILOS) > results/omp_sim_$$(hostname).csv
	@echo "CSV guardado en results/omp_sim_$$(hostname).csv"

demo-omp: $(DEMO_OMP) | results
	OMP_NUM_THREADS=4 ./$(DEMO_OMP) 10 4 | tee results/demo_omp_salida.txt

go-test:
	cd go && go test -v ./...

go-bench: | results
	cd go && go test ./... > /dev/null || (echo "La verificación Go falla: no se mide nada." && exit 1)
	cd go && go run . -reps 5 -hilos $(HILOS) -n 256,512,1024 > ../results/go_$$(hostname).csv
	@echo "CSV guardado en results/go_$$(hostname).csv"

figuras-s7: | results
	$(PY) scripts/graficos_s7.py

# ---------------- Semanas 9 a 11: MPI ----------------

mpi: $(TEST_MPI) $(BENCH_MPI) $(BENCH_SIM_MPI) $(DEMO_MPI) $(CARRERA_MPI) $(MOTOR_MPI)

# El motor MPI del programa visual (VISUAL.bat): habla con el visual por un socket local
ifeq ($(OS),Windows_NT)
  SOCKLIBS = -lws2_32
else
  SOCKLIBS =
endif
$(MOTOR_MPI): apps/motor_mpi.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) $(INC) apps/motor_mpi.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(SOCKLIBS) $(LDLIBS)

$(CARRERA_MPI): apps/carrera_mpi.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) $(INC) apps/carrera_mpi.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(LDLIBS)

$(TEST_MPI): tests/test_mpi.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) $(INC) tests/test_mpi.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(LDLIBS)

$(BENCH_MPI): bench/bench_mpi.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) $(INC) bench/bench_mpi.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(LDLIBS)

$(BENCH_SIM_MPI): bench/bench_sim_mpi.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) $(INC) bench/bench_sim_mpi.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(LDLIBS)

$(DEMO_MPI): apps/demo_sismo.c $(LIB_SRC) $(MPI_SRC) $(MPI_HEADERS) | $(BUILD)
	$(MPICC) $(CFLAGS) $(MPIFLAGS) -DUSE_MPI $(INC) apps/demo_sismo.c $(LIB_SRC) $(MPI_SRC) -o $@ $(MPILIBS) $(LDLIBS)

test-mpi: $(TEST_MPI)
	@for p in $(PROCS_TEST); do \
	  $(MPIEXEC) $$p ./$(TEST_MPI) > $(BUILD)/test_mpi_p$$p.txt 2>&1; rc=$$?; \
	  tail -n 1 $(BUILD)/test_mpi_p$$p.txt; \
	  if [ $$rc -ne 0 ]; then echo "FALLA con p = $$p: ver $(BUILD)/test_mpi_p$$p.txt"; exit 1; fi; \
	done
	@echo "Detalle de cada corrida en $(BUILD)/test_mpi_p<N>.txt"

bench-mpi: $(TEST_MPI) $(BENCH_MPI) $(BENCH_SIM_MPI) | results
	@for p in 1 4; do $(MPIEXEC) $$p ./$(TEST_MPI) > /dev/null || (echo "La verificación MPI falla: no se mide nada." && exit 1); done
	@h=$$(hostname); first=1; for p in $(PROCS); do \
	  if [ $$first = 1 ]; then $(MPIEXEC) $$p ./$(BENCH_MPI) 5 $(TAMANOS) > results/mpi_$$h.csv || exit 1; \
	  else $(MPIEXEC) $$p ./$(BENCH_MPI) 5 $(TAMANOS) -noheader >> results/mpi_$$h.csv || exit 1; fi; \
	  first=0; \
	done; echo "CSV guardado en results/mpi_$$h.csv"
	@h=$$(hostname); first=1; for p in $(PROCS); do \
	  if [ $$first = 1 ]; then $(MPIEXEC) $$p ./$(BENCH_SIM_MPI) 5 400 8 > results/mpi_sim_$$h.csv || exit 1; \
	  else $(MPIEXEC) $$p ./$(BENCH_SIM_MPI) 5 400 8 -noheader >> results/mpi_sim_$$h.csv || exit 1; fi; \
	  first=0; \
	done; echo "CSV guardado en results/mpi_sim_$$h.csv"

demo-mpi: $(DEMO_MPI) | results
	$(MPIEXEC) 4 ./$(DEMO_MPI) 10 4 | tee results/demo_mpi_salida.txt

carrera-mpi: $(CARRERA_MPI)
	@for p in 1 2 4 9; do $(MPIEXEC) $$p ./$(CARRERA_MPI) 100 400 20 || exit 1; done

figuras-s11: | results
	$(PY) scripts/graficos_s11.py

clean:
	rm -rf $(BUILD)
