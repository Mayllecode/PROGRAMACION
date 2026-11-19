// main.go — Mide S(p) y E(p) del worker pool en Go (semana 7).
//
// Uso (desde la carpeta go/):
//
//	go run . -reps 5 -hilos 1,2,4,8 -n 256,512,1024 > ../results/go_mipc.csv
//
// Mismo protocolo que bench/bench_omp.c (docs/PROTOCOLO_MEDICION.md):
//   - T(1) = versión secuencial en Go (MatmulIKJ o SpmmCSR), medida en la
//     misma corrida.
//   - 1 ejecución de calentamiento por caso, que se VERIFICA contra la
//     secuencial (idéntica bit a bit); si falla, no se mide nada.
//   - Mediana de -reps repeticiones; S(p) = T(1)/T(p), E(p) = S(p)/p.
//
// CSV (stdout): algoritmo,n,repeticion,tiempo_s,gflops,p
// El resumen con S(p) y E(p) sale por stderr.
package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"
)

func lista(s string) ([]int, error) {
	var out []int
	for _, campo := range strings.Split(s, ",") {
		v, err := strconv.Atoi(strings.TrimSpace(campo))
		if err != nil || v < 1 {
			return nil, fmt.Errorf("valor no válido %q en %q", campo, s)
		}
		out = append(out, v)
	}
	return out, nil
}

func mediana(v []float64) float64 {
	c := append([]float64(nil), v...)
	sort.Float64s(c)
	n := len(c)
	if n%2 == 1 {
		return c[n/2]
	}
	return 0.5 * (c[n/2-1] + c[n/2])
}

// lote es la duración mínima de cada medición. En Windows el reloj de Go
// avanza de a ~1 ms: un producto de 0.3 ms mediría 0 o 1 ms. Por eso, si una
// ejecución es corta, se mide un lote de varias seguidas y se divide.
const lote = 50 * time.Millisecond

// medir corre f una vez sin medir (calentamiento: la memoria de C se toca por
// primera vez aquí y no dentro del cronómetro), elige cuántas ejecuciones
// seguidas forman un lote de al menos 50 ms, y mide reps lotes; imprime el
// tiempo por ejecución de cada lote en CSV y devuelve la mediana
func medir(nombre string, n, p, reps int, flops float64, f func()) float64 {
	f()
	veces := 1
	for {
		t0 := time.Now()
		for i := 0; i < veces; i++ {
			f()
		}
		if time.Since(t0) >= lote || veces >= 1<<20 {
			break
		}
		veces *= 2
	}
	t := make([]float64, reps)
	for r := 0; r < reps; r++ {
		t0 := time.Now()
		for i := 0; i < veces; i++ {
			f()
		}
		t[r] = time.Since(t0).Seconds() / float64(veces)
		fmt.Printf("%s,%d,%d,%.6f,%.3f,%d\n", nombre, n, r+1, t[r], flops/t[r]*1e-9, p)
	}
	return mediana(t)
}

func fallar(format string, a ...any) {
	fmt.Fprintf(os.Stderr, "ERROR: "+format+"\n", a...)
	os.Exit(1)
}

func main() {
	reps := flag.Int("reps", 5, "repeticiones medidas por caso")
	hilosStr := flag.String("hilos", "1,2,4,8", "trabajadores (goroutines) del pool, separados por comas")
	nStr := flag.String("n", "256,512,1024", "tamaños del producto denso n x n")
	ladoStr := flag.String("lado", "128,256,512", "lados de la malla de rigidez 2D (n = lado²)")
	k := flag.Int("k", 16, "escenarios: columnas de U en K·U")
	flag.Parse()
	hilos, err := lista(*hilosStr)
	if err != nil {
		fallar("-hilos: %v", err)
	}
	tams, err := lista(*nStr)
	if err != nil {
		fallar("-n: %v", err)
	}
	lados, err := lista(*ladoStr)
	if err != nil {
		fallar("-lado: %v", err)
	}
	if *reps < 1 || *k < 1 {
		fallar("-reps y -k deben ser >= 1")
	}

	fmt.Println("algoritmo,n,repeticion,tiempo_s,gflops,p")
	fmt.Fprintf(os.Stderr, "Go %s: %d nucleos logicos (GOMAXPROCS = %d); mediana de %d repeticiones\n",
		runtime.Version(), runtime.NumCPU(), runtime.GOMAXPROCS(0), *reps)

	// ---- Denso: C = A·B, n x n ----
	for _, n := range tams {
		A, B := GenAleatoria(n, n, 1), GenAleatoria(n, n, 2)
		R, C := NuevaMatriz(n, n), NuevaMatriz(n, n)
		flops := 2.0 * float64(n) * float64(n) * float64(n)
		MatmulIKJ(A, B, R) // calentamiento y referencia
		T1 := medir("go_ikj", n, 1, *reps, flops, func() { MatmulIKJ(A, B, C) })
		fmt.Fprintf(os.Stderr, "\nDenso n = %d: T(1) = %.4f s (go_ikj secuencial)\n", n, T1)
		fmt.Fprintf(os.Stderr, "  %-14s %3s %11s %9s %7s %7s\n", "algoritmo", "p", "mediana_s", "GFLOP/s", "S(p)", "E(p)")
		for _, p := range hilos {
			fpt := FilasPorTarea(n, p)
			MatmulPool(A, B, C, p, fpt)
			if !Iguales(R, C) {
				fallar("go_pool n=%d p=%d no da lo mismo que la secuencial", n, p)
			}
			med := medir("go_pool", n, p, *reps, flops, func() { MatmulPool(A, B, C, p, fpt) })
			fmt.Fprintf(os.Stderr, "  %-14s %3d %11.4f %9.3f %7.2f %7.2f\n", "go_pool", p, med, flops/med*1e-9, T1/med, T1/med/float64(p))
		}
	}

	// ---- Disperso: K (rigidez 2D) · U (n x k) ----
	for _, lado := range lados {
		K := GenRigidez2D(lado, lado)
		n := K.Filas
		U, R, C := GenAleatoria(n, *k, 3), NuevaMatriz(n, *k), NuevaMatriz(n, *k)
		flops := 2.0 * float64(len(K.Val)) * float64(*k)
		SpmmCSR(K, U, R)
		nombre := fmt.Sprintf("go_spmm_k%d", *k)
		T1 := medir(nombre, n, 1, *reps, flops, func() { SpmmCSR(K, U, C) })
		fmt.Fprintf(os.Stderr, "\nDisperso K*U, malla %dx%d (n = %d, nnz = %d, %d escenarios): T(1) = %.5f s\n",
			lado, lado, n, len(K.Val), *k, T1)
		fmt.Fprintf(os.Stderr, "  %-14s %3s %11s %9s %7s %7s\n", "algoritmo", "p", "mediana_s", "GFLOP/s", "S(p)", "E(p)")
		for _, p := range hilos {
			fpt := FilasPorTarea(n, p)
			SpmmPool(K, U, C, p, fpt)
			if !Iguales(R, C) {
				fallar("go_spmm_pool n=%d p=%d no da lo mismo que la secuencial", n, p)
			}
			med := medir(fmt.Sprintf("go_spmm_pool_k%d", *k), n, p, *reps, flops, func() { SpmmPool(K, U, C, p, fpt) })
			fmt.Fprintf(os.Stderr, "  %-14s %3d %11.5f %9.3f %7.2f %7.2f\n", "go_spmm_pool", p, med, flops/med*1e-9, T1/med, T1/med/float64(p))
		}
	}
	fmt.Fprintln(os.Stderr, "\nS(p) = T(1)/T(p) con T(1) = la secuencial en Go;  E(p) = S(p)/p.")
}
