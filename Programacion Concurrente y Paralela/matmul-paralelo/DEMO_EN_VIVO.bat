@echo off
chcp 65001 >nul
cd /d "%~dp0"
set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;C:\Program Files\Go\bin;%PATH%"
if not exist build\bench_sim_omp.exe call :compilar

cls
echo ================================================================
echo  PASO 1 de 6 - PRIMERO CORRECTO: el secuencial pasa sus pruebas (H1)
echo ================================================================
build\test_seq.exe | findstr /C:"Resultado"
build\test_sim.exe | findstr /C:"periodo" /C:"Resultado"
echo.
pause

cls
echo ================================================================
echo  PASO 2 de 6 - OPENMP DA LO MISMO QUE EL SECUENCIAL (1, 2, 3, 4 y 8 hilos)
echo ================================================================
build\test_omp.exe | findstr /C:"Resultado"
echo.
pause

cls
echo ================================================================
echo  PASO 3 de 6 - GO DA LO MISMO QUE EL SECUENCIAL Y QUE C
echo ================================================================
pushd go
go test -v ./... 2>&1 | findstr /B /C:"--- " /C:"ok" /C:"FAIL"
popd
echo.
pause

cls
echo ================================================================
echo  PASO 4 de 6 - DEMO: edificio de 10 pisos x 4 vanos ante 4 sismos (OpenMP, 4 hilos)
echo ================================================================
set OMP_NUM_THREADS=4
build\demo_sismo_omp.exe 10 4
set OMP_NUM_THREADS=
echo.
pause

cls
echo ================================================================
echo  PASO 5 de 6 - SPEEDUP: 1, 2, 4 y 8 hilos (OpenMP) y goroutines (Go)
echo ================================================================
build\bench_omp.exe 1 1,2,4,8 1024 >nul
echo.
pause
pushd go
go run . -reps 1 -n 1024 -lado 256 >nul
popd
echo.
pause

cls
echo ================================================================
echo  PASO 6 de 6 - EL MISMO SISMO EN 3 EDIFICIOS: chico, mediano y grande
echo  Mira sim_omp con p = 8: en el chico es MAS LENTO, en el grande ~2x MAS RAPIDO
echo ================================================================
build\bench_sim_omp.exe 3 400 8 1,8 >nul
echo.
echo FIN DE LA DEMO
pause
goto :eof

:compilar
echo Compilando por primera vez (unos segundos)...
if not exist build mkdir build
set "F=-O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude"
set "L=src/matrix.c src/generators.c src/seq.c src/verify.c src/timer.c src/building.c src/ground_motion.c src/sim.c"
set "O=src/omp/matmul_omp.c src/omp/sim_omp.c"
gcc %F% tests/test_seq.c %L% -o build/test_seq.exe -lm
gcc %F% tests/test_sim.c %L% -o build/test_sim.exe -lm
gcc %F% -fopenmp tests/test_omp.c %L% %O% -o build/test_omp.exe -lm
gcc %F% -fopenmp apps/demo_sismo.c %L% %O% -o build/demo_sismo_omp.exe -lm
gcc %F% -fopenmp bench/bench_omp.c %L% %O% -o build/bench_omp.exe -lm
gcc %F% -fopenmp bench/bench_sim_omp.c %L% %O% -o build/bench_sim_omp.exe -lm
goto :eof
