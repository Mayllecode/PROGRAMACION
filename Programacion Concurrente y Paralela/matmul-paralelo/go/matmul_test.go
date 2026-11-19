// matmul_test.go — Pruebas de la versión Go (semana 7, Hito H2).
//
// Criterio del Hito H2: "Go coincide con el secuencial con 1 a 8 hilos".
// Como en tests/test_omp.c, la comparación es EXACTA: cada tarea del pool
// suma en el mismo orden que la secuencial.
//
// Correr (desde la carpeta go/):
//
//	go test -v              todas las pruebas, una línea por prueba
//	go test -race           además busca condiciones de carrera (necesita gcc en el PATH)
//	go test -bench . -run ^$   mide el pool con 1, 2, 4 y 8 trabajadores (n = 512)
package main

import (
	"errors"
	"fmt"
	"testing"
)

var trabajadores = []int{1, 2, 3, 4, 8}

func TestEjemploAMano(t *testing.T) {
	A := &Matriz{2, 3, []float64{1, 2, 3, 4, 5, 6}}
	B := &Matriz{3, 2, []float64{7, 8, 9, 10, 11, 12}}
	esperado := []float64{58, 64, 139, 154}
	C := NuevaMatriz(2, 2)
	MatmulIKJ(A, B, C)
	if !Iguales(C, &Matriz{2, 2, esperado}) {
		t.Fatalf("secuencial: %v, se esperaba %v", C.Datos, esperado)
	}
	C = NuevaMatriz(2, 2)
	MatmulPool(A, B, C, 8, 1) // más trabajadores que filas
	if !Iguales(C, &Matriz{2, 2, esperado}) {
		t.Fatalf("pool con 8 trabajadores: %v, se esperaba %v", C.Datos, esperado)
	}
}

// Valores impresos por el programa en C (gen_random y matmul_ikj) con las
// mismas semillas: Go genera las mismas matrices y obtiene el mismo producto.
func TestGeneradorIgualAC(t *testing.T) {
	desdeC := []float64{0.13312315034456179, 0.49156351452540226, 0.94200550717359244,
		-0.11128156588845584, -0.1114705983472839, 0.52578878382352201}
	A := GenAleatoria(2, 3, 1)
	for i, v := range desdeC {
		if A.Datos[i] != v {
			t.Fatalf("gen_random(2,3,1)[%d]: Go %.17g, C %.17g", i, A.Datos[i], v)
		}
	}
	X, Y, Z := GenAleatoria(64, 64, 1), GenAleatoria(64, 64, 2), NuevaMatriz(64, 64)
	MatmulIKJ(X, Y, Z)
	suma := 0.0
	for _, v := range Z.Datos {
		suma += v
	}
	if suma != -43.821294432278947 || Z.At(5, 7) != -6.1498538095961717 {
		t.Fatalf("producto 64x64: suma %.17g y z[5][7] %.17g no coinciden con C", suma, Z.At(5, 7))
	}
}

func TestPoolIgualSecuencial(t *testing.T) {
	dims := [][3]int{{37, 37, 37}, {50, 23, 61}, {129, 65, 100}} // n x p  *  p x m
	for _, p := range trabajadores {
		for _, fpt := range []int{1, 7, 64} {
			t.Run(fmt.Sprintf("p=%d_filasPorTarea=%d", p, fpt), func(t *testing.T) {
				for d, dm := range dims {
					A := GenAleatoria(dm[0], dm[1], uint64(11+d))
					B := GenAleatoria(dm[1], dm[2], uint64(21+d))
					R, C := NuevaMatriz(dm[0], dm[2]), NuevaMatriz(dm[0], dm[2])
					for i := range C.Datos {
						C.Datos[i] = 99 // basura: el pool tiene que sobrescribirla toda
					}
					MatmulIKJ(A, B, R)
					MatmulPool(A, B, C, p, fpt)
					if !Iguales(R, C) {
						t.Fatalf("%dx%d * %dx%d: %v", dm[0], dm[1], dm[1], dm[2], Comparar(R, C, 1e-9))
					}
				}
			})
		}
	}
}

func TestSpmm(t *testing.T) {
	K := GenRigidez2D(37, 23)
	U := GenAleatoria(K.Cols, 16, 31)
	R := NuevaMatriz(K.Filas, 16)
	SpmmCSR(K, U, R)

	// 1) Contra el producto denso de la misma K
	D := NuevaMatriz(K.Filas, 16)
	MatmulIKJ(K.ADensa(), U, D)
	if err := Comparar(D, R, 1e-9); err != nil {
		t.Fatalf("SpmmCSR vs denso: %v", err)
	}
	// 2) K·1: la suma de cada fila es 4 - (número de vecinos): 0 en el interior
	unos := NuevaMatriz(K.Cols, 1)
	for i := range unos.Datos {
		unos.Datos[i] = 1
	}
	K1 := NuevaMatriz(K.Filas, 1)
	SpmmCSR(K, unos, K1)
	if v := K1.At(5*37+5, 0); v != 0 {
		t.Fatalf("K·1 en un nodo interior da %g, se esperaba 0", v)
	}
	// 3) El pool, con 1 a 8 trabajadores, idéntico a la secuencial
	for _, p := range trabajadores {
		C := NuevaMatriz(K.Filas, 16)
		SpmmPool(K, U, C, p, FilasPorTarea(K.Filas, p))
		if !Iguales(R, C) {
			t.Fatalf("SpmmPool con p=%d: %v", p, Comparar(R, C, 1e-9))
		}
	}
}

func TestErrores(t *testing.T) {
	A, B, C := NuevaMatriz(4, 3), NuevaMatriz(4, 2), NuevaMatriz(4, 2)
	if !errors.Is(MatmulIKJ(A, B, C), ErrDimensiones) || !errors.Is(MatmulPool(A, B, C, 4, 1), ErrDimensiones) {
		t.Fatal("A.Cols != B.Filas debería rechazarse")
	}
	K := GenRigidez2D(2, 2)
	if !errors.Is(SpmmPool(K, NuevaMatriz(3, 2), C, 4, 1), ErrDimensiones) {
		t.Fatal("K de 4 columnas por U de 3 filas debería rechazarse")
	}
}

func BenchmarkMatmulPool512(b *testing.B) {
	A, B, C := GenAleatoria(512, 512, 1), GenAleatoria(512, 512, 2), NuevaMatriz(512, 512)
	for _, p := range []int{1, 2, 4, 8} {
		b.Run(fmt.Sprintf("p=%d", p), func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				MatmulPool(A, B, C, p, FilasPorTarea(512, p))
			}
		})
	}
}
