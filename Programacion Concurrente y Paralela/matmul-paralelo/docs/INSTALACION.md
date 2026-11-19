# Instalación del entorno

Qué hace falta en cada fase y cómo instalarlo. Al final, cada integrante corre
`make env` (o `mingw32-make env` en Windows) y sube su `results/entorno_<nombre>.txt`.

| Herramienta | Para qué | Fase | Comprobar |
|---|---|---|---|
| **gcc** (C11) + **make** | compilar todo el proyecto | S5–S6 (ya) | `gcc --version` |
| **Python 3 + matplotlib** | gráficos (`make figuras`) | S6 (ya) | `python -c "import matplotlib"` |
| **OpenMP** (viene con gcc) | paralelismo en memoria compartida | S7 | `make env` → `[OK] OpenMP compila` |
| **Go** | variante con *worker pool* de goroutines | S7 | `go version` |
| **MPI** (Open MPI) | memoria distribuida: Cannon, SUMMA, simulación repartida | S9 | `mpirun --version` |
| **CUDA** (nvcc) | producto K·U en la GPU | S12 | `nvcc --version` |

Hay tres caminos. **Para S5–S6 basta el A** (Windows nativo); para MPI (S9) hace
falta el B (WSL2) o una máquina Linux.

---

## A. Windows nativo con WinLibs (gcc + make + OpenMP) — probado en DESKTOP-R3PLOD9

1. **Instalar gcc** (incluye `mingw32-make` y OpenMP). En PowerShell:
   ```powershell
   winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
   ```
   Cerrar y volver a abrir la terminal para que tome el PATH. Comprobar con
   `gcc --version` (debe decir *MinGW-W64 x86_64-ucrt-posix-seh*).
   Sin winget: bajar el `.zip` "UCRT runtime, POSIX threads" de
   <https://winlibs.com>, descomprimirlo en `C:\mingw64` y agregar
   `C:\mingw64\bin` al PATH.
2. **Abrir Git Bash** (viene con Git for Windows) en la carpeta del proyecto:
   el `Makefile` usa comandos de shell POSIX (`mkdir -p`, `tee`, `hostname`).
   Desde PowerShell o cmd no funciona tal cual.
3. **Python y matplotlib** (para los gráficos):
   ```bash
   pip install matplotlib
   ```
4. Probar:
   ```bash
   mingw32-make test       # 70 pruebas OK
   mingw32-make entregable # test + validar + demo + bench + figuras
   ```
   Para escribir `make` en vez de `mingw32-make`: `alias make=mingw32-make`
   (o agregarlo a `~/.bashrc`).

## B. WSL2 con Ubuntu (necesario para MPI desde S9)

En esta PC Ubuntu ya está instalado, pero WSL2 **no arranca**:

```
WSL2 no se puede iniciar porque la virtualización no está habilitada ...
Código de error: Wsl/Service/CreateInstance/CreateVm/HCS/HCS_E_HYPERV_NOT_INSTALLED
```

La virtualización **sí** está activa en la BIOS (`VirtualizationFirmwareEnabled: True`);
lo que falta es el componente de Windows **Plataforma de máquina virtual**:

1. PowerShell **como administrador**:
   ```powershell
   wsl.exe --install --no-distribution
   ```
   (equivale a `dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart`)
2. **Reiniciar** la PC.
3. Abrir Ubuntu y, dentro:
   ```bash
   sudo apt update
   sudo apt install build-essential git openmpi-bin libopenmpi-dev golang-go python3-matplotlib
   cd "/mnt/c/Users/User/Desktop/Matrices paralelas/matmul-paralelo"
   make env && make test
   ```
   Si `make env` sigue marcando la BIOS como causa, entrar al *setup* (tecla
   Supr o F2 al encender) y activar **Intel Virtualization Technology (VT-x)**.

> No hacerlo justo antes de una exposición: requiere reiniciar.

## C. Google Colab (respaldo, y CUDA si no hay GPU local)

En un cuaderno nuevo: *Entorno de ejecución → Cambiar tipo → GPU*. Luego:

```bash
!git clone <url-del-repositorio> && cd matmul-paralelo && make test
!nvcc --version
```

## CUDA en esta PC (S12)

`make env` detectó una **NVIDIA GeForce GTX 1660 SUPER** (arquitectura Turing,
6 GB, capacidad de cómputo 7.5): la parte CUDA se puede hacer en local.

- **Opción recomendada:** CUDA Toolkit dentro de WSL2 (camino B), así MPI y CUDA
  conviven en el mismo entorno. El driver de NVIDIA de Windows ya da acceso a la
  GPU desde WSL2; se instala solo el *toolkit* (paquete "WSL-Ubuntu" de la página
  de descargas de NVIDIA, **no** un driver Linux).
- **En Windows nativo:** `nvcc` necesita el compilador de Microsoft (MSVC):
  instalar *Visual Studio Build Tools* con "Desarrollo para el escritorio con
  C++" y luego el CUDA Toolkit. Compilar con `nvcc -arch=sm_75`.
- **Respaldo:** Colab (camino C).

## Problemas frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| `gcc: command not found` | gcc no está en el PATH | Reabrir la terminal tras instalar; revisar `where gcc` |
| `make: command not found` en Git Bash | WinLibs trae `mingw32-make`, no `make` | Usar `mingw32-make` o `alias make=mingw32-make` |
| `mkdir -p` o `./build/...` fallan | Se corrió `mingw32-make` desde PowerShell/cmd | Correrlo desde Git Bash |
| `HCS_E_HYPERV_NOT_INSTALLED` al abrir Ubuntu | Falta "Plataforma de máquina virtual" | Paso B.1 y reiniciar |
| `make figuras` dice "Falta matplotlib" | No está instalado para ese Python | `pip install matplotlib` (Windows) o `sudo apt install python3-matplotlib` |
| `mpirun` no lanza 4 procesos | Menos núcleos que procesos | `mpirun --oversubscribe -np 4 ...` |
| Los tiempos varían mucho entre corridas | Otros programas abiertos, portátil a batería | Ver `docs/PROTOCOLO_MEDICION.md` §1 |
