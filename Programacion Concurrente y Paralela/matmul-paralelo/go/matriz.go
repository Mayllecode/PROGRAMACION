// matriz.go — Estructuras y generadores, equivalentes a include/matrix.h y
// include/generators.h de la versión en C.
//
// Los generadores usan el MISMO splitmix64 que src/generators.c: con la misma
// semilla, Go y C producen exactamente las mismas matrices (lo comprueba
// TestGeneradorIgualAC). Así los dos lenguajes se pueden comparar con los
// mismos datos.
package main

import (
	"fmt"
	"math"
)

// Matriz densa en orden por filas: el elemento (i, j) está en Datos[i*Cols+j].
type Matriz struct {
	Filas, Cols int
	Datos       []float64
}

// CSR: matriz dispersa (Compressed Sparse Row). Los valores de la fila i
// están en Val[FilaPtr[i]:FilaPtr[i+1]] y sus columnas en ColIdx (mismo rango).
type CSR struct {
	Filas, Cols int
	FilaPtr     []int
	ColIdx      []int
	Val         []float64
}

func NuevaMatriz(filas, cols int) *Matriz {
	return &Matriz{Filas: filas, Cols: cols, Datos: make([]float64, filas*cols)}
}

func (m *Matriz) At(i, j int) float64 { return m.Datos[i*m.Cols+j] }

// fila devuelve la fila i como slice (sin copiar)
func (m *Matriz) fila(i int) []float64 { return m.Datos[i*m.Cols : (i+1)*m.Cols] }

type splitmix64 uint64

func (s *splitmix64) siguiente() uint64 {
	*s += 0x9E3779B97F4A7C15
	z := uint64(*s)
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

// uniforme en [0, 1) con 53 bits de precisión (como urand en C)
func (s *splitmix64) uniforme() float64 {
	return float64(s.siguiente()>>11) * (1.0 / 9007199254740992.0)
}

// GenAleatoria: densa filas x cols con valores uniformes en [-1, 1) (gen_random en C).
func GenAleatoria(filas, cols int, semilla uint64) *Matriz {
	s := splitmix64(semilla)
	m := NuevaMatriz(filas, cols)
	for t := range m.Datos {
		m.Datos[t] = 2.0*s.uniforme() - 1.0
	}
	return m
}

// GenRigidez2D: rigidez de una malla de nx x ny nodos, esténcil de 5 puntos
// (diagonal 4, vecinos -1), igual que gen_stiffness_2d en C.
func GenRigidez2D(nx, ny int) *CSR {
	n := nx * ny
	nnz := n + 2*((nx-1)*ny+nx*(ny-1))
	K := &CSR{Filas: n, Cols: n, FilaPtr: make([]int, n+1), ColIdx: make([]int, 0, nnz), Val: make([]float64, 0, nnz)}
	agregar := func(col int, v float64) {
		K.ColIdx = append(K.ColIdx, col)
		K.Val = append(K.Val, v)
	}
	for y := 0; y < ny; y++ {
		for x := 0; x < nx; x++ {
			i := y*nx + x
			K.FilaPtr[i] = len(K.Val)
			if y > 0 {
				agregar(i-nx, -1)
			}
			if x > 0 {
				agregar(i-1, -1)
			}
			agregar(i, 4)
			if x < nx-1 {
				agregar(i+1, -1)
			}
			if y < ny-1 {
				agregar(i+nx, -1)
			}
		}
	}
	K.FilaPtr[n] = len(K.Val)
	return K
}

// ADensa convierte una CSR en densa (para las pruebas).
func (K *CSR) ADensa() *Matriz {
	m := NuevaMatriz(K.Filas, K.Cols)
	for i := 0; i < K.Filas; i++ {
		for t := K.FilaPtr[i]; t < K.FilaPtr[i+1]; t++ {
			m.Datos[i*K.Cols+K.ColIdx[t]] = K.Val[t]
		}
	}
	return m
}

// Iguales: comparación exacta, elemento por elemento.
func Iguales(a, b *Matriz) bool {
	if a.Filas != b.Filas || a.Cols != b.Cols {
		return false
	}
	for t := range a.Datos {
		if a.Datos[t] != b.Datos[t] {
			return false
		}
	}
	return true
}

// Comparar: mismo criterio que verify.h, |x - ref| <= eps·(1 + |ref|).
// Devuelve nil si todo pasa, o un error con el primer elemento que falla.
func Comparar(ref, x *Matriz, eps float64) error {
	if ref.Filas != x.Filas || ref.Cols != x.Cols {
		return fmt.Errorf("dimensiones distintas: %dx%d vs %dx%d", ref.Filas, ref.Cols, x.Filas, x.Cols)
	}
	for t, a := range ref.Datos {
		d := math.Abs(a - x.Datos[t])
		if !(d <= eps*(1+math.Abs(a))) { // también atrapa NaN
			return fmt.Errorf("primer error en (%d,%d): ref=%g x=%g", t/ref.Cols, t%ref.Cols, a, x.Datos[t])
		}
	}
	return nil
}
