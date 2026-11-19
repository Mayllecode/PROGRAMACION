#!/usr/bin/env bash
# check_env.sh — Revisa que la máquina tenga todo lo necesario para el
# proyecto y guarda el reporte en results/entorno.txt.
# Ese archivo se adjunta a cada medición: sin saber en qué CPU se midió,
# un tiempo no significa nada.

mkdir -p results
OUT=results/entorno.txt
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

ok()   { echo "  [OK]    $1"; }
falta(){ echo "  [FALTA] $1"; }

{
echo "=== Entorno del proyecto — $(date '+%Y-%m-%d %H:%M') — $(hostname)"
echo
echo "--- Sistema"
echo "  $(uname -srm)"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
  *) WINDOWS=0 ;;
esac
if command -v lscpu >/dev/null 2>&1; then
  lscpu | grep -E '^(Model name|CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|L1d cache|L2 cache|L3 cache)' | sed 's/^/  /'
elif [ "$WINDOWS" = 1 ] && command -v powershell.exe >/dev/null 2>&1; then
  powershell.exe -NoProfile -Command "\$p = Get-CimInstance Win32_Processor; \$c = Get-CimInstance Win32_ComputerSystem; 'CPU: ' + \$p.Name; 'Nucleos fisicos: ' + \$p.NumberOfCores + '   hilos: ' + \$p.NumberOfLogicalProcessors; 'Cache L2: ' + \$p.L2CacheSize + ' KB   L3: ' + \$p.L3CacheSize + ' KB'; 'RAM: ' + [math]::Round(\$c.TotalPhysicalMemory / 1GB, 1) + ' GB'" 2>/dev/null | tr -d '' | sed 's/^/  /'
elif command -v sysctl >/dev/null 2>&1; then
  echo "  $(sysctl -n machdep.cpu.brand_string 2>/dev/null)  núcleos: $(sysctl -n hw.ncpu 2>/dev/null)"
fi
if command -v free >/dev/null 2>&1; then echo "  RAM: $(free -h | awk '/Mem:/ {print $2}')"; fi

echo
echo "--- Compilador C y OpenMP (Fase 2)"
if command -v gcc >/dev/null 2>&1; then
  ok "$(gcc --version | head -1)"
  cat > "$TMP/omp.c" << 'C'
#include <omp.h>
#include <stdio.h>
int main(void){ int n=0;
#pragma omp parallel
{
#pragma omp atomic
n++;
}
printf("%d hilos (máximo disponible: %d)\n", n, omp_get_max_threads()); return 0; }
C
  if gcc -fopenmp "$TMP/omp.c" -o "$TMP/omp" 2>/dev/null; then
    ok "OpenMP compila; prueba con OMP_NUM_THREADS=4 -> $(OMP_NUM_THREADS=4 "$TMP/omp")"
  else
    falta "OpenMP no compila (en macOS: brew install libomp, o usar gcc de Homebrew)"
  fi
else
  falta "gcc (Ubuntu/WSL: sudo apt install build-essential)"
fi

echo
echo "--- Python + matplotlib (gráficos: make figuras)"
PYBIN=$(command -v python3 || command -v python)
if [ -n "$PYBIN" ] && "$PYBIN" -c "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)" 2>/dev/null; then
  ok "$("$PYBIN" --version 2>&1)"
  if "$PYBIN" -c "import matplotlib" 2>/dev/null; then ok "matplotlib $("$PYBIN" -c 'import matplotlib; print(matplotlib.__version__)')"
  else falta "matplotlib (pip install matplotlib   o   sudo apt install python3-matplotlib)"; fi
else
  falta "python3 (sudo apt install python3 python3-matplotlib)"
fi

echo
echo "--- Go (variante worker pool, Fase 2)"
if command -v go >/dev/null 2>&1; then ok "$(go version)"; else falta "go (sudo apt install golang-go)"; fi

echo
echo "--- MPI (Fase 3)"
if command -v mpicc >/dev/null 2>&1; then
  ok "mpicc: $(mpicc --version 2>/dev/null | head -1)"
  if command -v mpirun >/dev/null 2>&1; then ok "mpirun: $(mpirun --version 2>&1 | head -1)"; fi
  cat > "$TMP/mpi.c" << 'C'
#include <mpi.h>
#include <stdio.h>
int main(int c, char **v){ int r,s; MPI_Init(&c,&v); MPI_Comm_rank(MPI_COMM_WORLD,&r);
MPI_Comm_size(MPI_COMM_WORLD,&s); if(r==0) printf("%d procesos MPI respondieron\n", s);
MPI_Finalize(); return 0; }
C
  if mpicc "$TMP/mpi.c" -o "$TMP/mpi" 2>/dev/null; then
    RES=$(mpirun -np 4 "$TMP/mpi" 2>/dev/null || mpirun --oversubscribe -np 4 "$TMP/mpi" 2>/dev/null)
    if [ -n "$RES" ]; then ok "hola MPI con 4 procesos -> $RES"; else falta "mpirun no pudo lanzar 4 procesos"; fi
  else
    falta "mpicc no compila un programa de prueba"
  fi
else
  if [ "$WINDOWS" = 1 ]; then
    falta "MPI (en Windows: usar WSL2 con Ubuntu, ver docs/INSTALACION.md)"
  else
    falta "MPI (sudo apt install openmpi-bin libopenmpi-dev)"
  fi
fi

echo
echo "--- CUDA (Fase 4)"
if command -v nvcc >/dev/null 2>&1; then ok "$(nvcc --version | tail -1)"; else falta "nvcc (si no hay GPU local: usar Google Colab con entorno GPU)"; fi
if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi -L | sed 's/^/  [OK]    /'
else
  falta "nvidia-smi / GPU NVIDIA en esta máquina"
fi
} | tee "$OUT"

echo
echo "Reporte guardado en $OUT"
