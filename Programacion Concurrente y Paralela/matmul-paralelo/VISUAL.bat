@echo off
chcp 65001 > nul
rem SISMO PARALELO: el edificio animado + la carrera Secuencial vs OpenMP vs Go vs MPI.
rem Si faltan build\sismo_visual.exe o build\motor_mpi.exe, los compila
rem (necesita Go, gcc de WinLibs y MS-MPI con su SDK).
cd /d "%~dp0"
title SISMO PARALELO
set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;C:\Program Files\Go\bin;%PATH%"
if not exist build mkdir build
if not exist build\sismo_visual.exe call :visual
if not exist build\motor_mpi.exe call :motor_mpi
build\sismo_visual.exe
goto :eof

:visual
echo Compilando el programa visual...
set CGO_ENABLED=1
pushd visual
go build -ldflags "-extldflags=-static" -o ..\build\sismo_visual.exe .
popd
goto :eof

:motor_mpi
echo Compilando el motor MPI del visual...
set "MI=C:\Program Files (x86)\Microsoft SDKs\MPI\Include"
set "ML=C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64"
set "L=src/matrix.c src/generators.c src/seq.c src/verify.c src/timer.c src/building.c src/ground_motion.c src/sim.c"
set "M=src/mpi/mpi_common.c src/mpi/cannon.c src/mpi/summa.c src/mpi/sim_mpi.c"
gcc -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude -I"%MI%" apps/motor_mpi.c %L% %M% -o build/motor_mpi.exe -L"%ML%" -lmsmpi -lws2_32 -lm
goto :eof
