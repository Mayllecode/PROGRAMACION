@echo off
chcp 65001 >nul
rem DEMO POR CONSOLA de MPI (S8-S11, Hito H3): 1, 4, 9 y 16 procesos. El visual (con MPI) esta en VISUAL.bat
rem Doble clic, o desde una terminal en esta carpeta: DEMO_MPI.bat
cd /d "%~dp0"
title DEMO MPI - Simulacion sismica
set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;%PATH%"
if "%MSMPI_BIN%"=="" set "MSMPI_BIN=C:\Program Files\Microsoft MPI\Bin\"
set "MPIEXEC=%MSMPI_BIN%mpiexec.exe"
if not exist "%MPIEXEC%" (
  echo No se encuentra mpiexec. Instalar MS-MPI: winget install -e --id Microsoft.msmpi
  pause
  goto :eof
)
if not exist build\test_mpi.exe call :compilar
if not exist build\demo_sismo.exe call :compilar
if not exist build\carrera_mpi.exe call :compilar

cls
echo ================================================================
echo  PASO 1 de 5 - CRITERIO DEL HITO H3: lo mismo con 1, 4, 9 y 16 procesos
echo  (Cannon y SUMMA dentro de eps; K*U y simulador IDENTICOS bit a bit)
echo ================================================================
for %%p in (1 4 9 16) do "%MPIEXEC%" -n %%p build\test_mpi.exe | findstr /C:"Resultado"
echo.
pause

cls
echo ================================================================
echo  PASO 2 de 5 - CANNON vs SUMMA: n = 1024 con 4 procesos (malla 2x2)
echo  Mira las columnas comm_s, mensajes y MB
echo ================================================================
"%MPIEXEC%" -n 4 build\bench_mpi.exe 3 1024 >nul
echo.
echo  Y con 9 procesos (malla 3x3):
"%MPIEXEC%" -n 9 build\bench_mpi.exe 3 1024 -noheader >nul
echo.
pause

cls
echo ================================================================
echo  PASO 3 de 5 - EL EDIFICIO REPARTIDO: 10 pisos x 4 vanos, 4 procesos
echo  Mira la tabla "proceso / pisos / fantasmas / vecinos" (Espacio = ver el resto)
echo ================================================================
"%MPIEXEC%" -n 4 build\demo_sismo_mpi.exe 10 4 | more
echo.
pause

cls
echo ================================================================
echo  PASO 4 de 5 - MISMO RESULTADO QUE EL SECUENCIAL (archivo por archivo)
echo ================================================================
build\demo_sismo.exe 10 4 >nul
copy /y results\respuesta.csv results\respuesta_seq.csv >nul
copy /y results\historia_techo.csv results\historia_seq.csv >nul
"%MPIEXEC%" -n 9 build\demo_sismo_mpi.exe 10 4 >nul
echo Secuencial contra MPI con 9 procesos:
fc /b results\respuesta.csv results\respuesta_seq.csv
fc /b results\historia_techo.csv results\historia_seq.csv
echo.
pause

cls
echo ================================================================
echo  PASO 5 de 5 - LA CARRERA: el mismo edificio con 1, 2, 4 y 9 procesos
echo  100 pisos x 400 vanos (40 100 nodos), 8 sismos a la vez, 20 s de sismo
echo ================================================================
for %%p in (1 2 4 9) do "%MPIEXEC%" -n %%p build\carrera_mpi.exe 100 400 20
echo.
echo FIN DE LA DEMO
pause
goto :eof

:compilar
echo Compilando por primera vez (unos segundos)...
if not exist build mkdir build
if not exist results mkdir results
set "F=-O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude"
set "MI=-I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include""
set "ML=-L"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" -lmsmpi"
set "L=src/matrix.c src/generators.c src/seq.c src/verify.c src/timer.c src/building.c src/ground_motion.c src/sim.c"
set "M=src/mpi/mpi_common.c src/mpi/cannon.c src/mpi/summa.c src/mpi/sim_mpi.c"
gcc %F% apps/demo_sismo.c %L% -o build/demo_sismo.exe -lm
gcc %F% %MI% tests/test_mpi.c %L% %M% -o build/test_mpi.exe %ML% -lm
gcc %F% %MI% bench/bench_mpi.c %L% %M% -o build/bench_mpi.exe %ML% -lm
gcc %F% %MI% bench/bench_sim_mpi.c %L% %M% -o build/bench_sim_mpi.exe %ML% -lm
gcc %F% %MI% -DUSE_MPI apps/demo_sismo.c %L% %M% -o build/demo_sismo_mpi.exe %ML% -lm
gcc %F% %MI% apps/carrera_mpi.c %L% %M% -o build/carrera_mpi.exe %ML% -lm
goto :eof
