// secuencial.go — Referencia secuencial en Go (como src/seq.c).
package main

import "errors"

var ErrDimensiones = errors.New("las dimensiones no calzan")

func dimsOK(af, ac int, B, C *Matriz) bool {
	return ac == B.Filas && C.Filas == af && C.Cols == B.Cols
}

// filasIKJ calcula las filas [ini, fin) de C = A·B en orden i-k-j.
// Es el trabajo de una tarea del worker pool (paralelo.go) y también, con
// [0, n), la versión secuencial.
//
// float64(a * b[j]): la conversión explícita impide que el compilador junte
// la multiplicación y la suma en una sola instrucción FMA (la especificación
// de Go lo permite y en ARM, por ejemplo una Mac M1, lo hace). Con FMA el
// resultado cambiaría en el último bit y ya no sería idéntico al de C.
func filasIKJ(A, B, C *Matriz, ini, fin int) {
	for i := ini; i < fin; i++ {
		c := C.fila(i)
		for j := range c {
			c[j] = 0
		}
		for k := 0; k < A.Cols; k++ {
			a := A.Datos[i*A.Cols+k]
			b := B.fila(k)
			for j := range c {
				c[j] += float64(a * b[j])
			}
		}
	}
}

// filasSpmm calcula las filas [ini, fin) de C = K·U con K en CSR.
func filasSpmm(K *CSR, U, C *Matriz, ini, fin int) {
	for i := ini; i < fin; i++ {
		c := C.fila(i)
		for j := range c {
			c[j] = 0
		}
		for t := K.FilaPtr[i]; t < K.FilaPtr[i+1]; t++ {
			a := K.Val[t]
			b := U.fila(K.ColIdx[t])
			for j := range c {
				c[j] += float64(a * b[j])
			}
		}
	}
}

// MatmulIKJ: C = A·B secuencial, orden i-k-j (matmul_ikj en C).
func MatmulIKJ(A, B, C *Matriz) error {
	if !dimsOK(A.Filas, A.Cols, B, C) {
		return ErrDimensiones
	}
	filasIKJ(A, B, C, 0, A.Filas)
	return nil
}

// SpmmCSR: C = K·U secuencial con K en CSR (spmm_csr_rowinit en C).
func SpmmCSR(K *CSR, U, C *Matriz) error {
	if !dimsOK(K.Filas, K.Cols, U, C) {
		return ErrDimensiones
	}
	filasSpmm(K, U, C, 0, K.Filas)
	return nil
}
