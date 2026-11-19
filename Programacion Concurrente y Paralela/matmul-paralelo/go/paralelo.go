// paralelo.go — Worker pool de goroutines (semana 7).
//
// Un worker pool es un grupo FIJO de goroutines ("trabajadores") que sacan
// tareas de una cola común hasta que se acaba:
//
//	main ──tareas──► [ canal ] ──► trabajador 1 ─┐
//	                            ├─► trabajador 2 ─┼─► escriben SUS filas de C
//	                            └─► trabajador p ─┘
//	main espera con un WaitGroup a que los p terminen
//
// Cada tarea es un rango de filas [ini, fin) de C. Dos trabajadores nunca
// reciben la misma tarea, así que nunca escriben la misma fila: no hace falta
// ningún Mutex. El canal reparte las tareas de forma DINÁMICA: el que termina
// antes toma la siguiente (en OpenMP sería schedule(dynamic)).
//
// Diferencia con OpenMP: en C, "#pragma omp parallel for" crea los hilos y
// reparte las iteraciones solo; en Go lo escribimos explícitamente con
// goroutines, un canal y un sync.WaitGroup.
package main

import "sync"

type tarea struct{ ini, fin int }

// pool reparte las filas [0, nFilas) en tareas de filasPorTarea filas entre
// `trabajadores` goroutines, que ejecutan calcular(ini, fin) con cada una.
func pool(nFilas, trabajadores, filasPorTarea int, calcular func(ini, fin int)) {
	if trabajadores < 1 {
		trabajadores = 1
	}
	if filasPorTarea < 1 {
		filasPorTarea = 1
	}
	tareas := make(chan tarea)
	var wg sync.WaitGroup
	for w := 0; w < trabajadores; w++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for t := range tareas { // hasta que el canal se cierre y se vacíe
				calcular(t.ini, t.fin)
			}
		}()
	}
	for ini := 0; ini < nFilas; ini += filasPorTarea {
		tareas <- tarea{ini, min(ini+filasPorTarea, nFilas)}
	}
	close(tareas) // "no hay más tareas": los trabajadores salen del for
	wg.Wait()     // barrera: C está completa cuando todos terminaron
}

// FilasPorTarea elige el tamaño de tarea: unas 8 tareas por trabajador.
// Muy pocas tareas -> mal balance; demasiadas -> el canal se vuelve el costo.
func FilasPorTarea(nFilas, trabajadores int) int {
	return max(1, nFilas/(8*max(1, trabajadores)))
}

// MatmulPool: C = A·B con un worker pool. Mismo resultado bit a bit que MatmulIKJ.
func MatmulPool(A, B, C *Matriz, trabajadores, filasPorTarea int) error {
	if !dimsOK(A.Filas, A.Cols, B, C) {
		return ErrDimensiones
	}
	pool(A.Filas, trabajadores, filasPorTarea, func(ini, fin int) { filasIKJ(A, B, C, ini, fin) })
	return nil
}

// SpmmPool: C = K·U (K en CSR) con un worker pool. Mismo resultado que SpmmCSR.
func SpmmPool(K *CSR, U, C *Matriz, trabajadores, filasPorTarea int) error {
	if !dimsOK(K.Filas, K.Cols, U, C) {
		return ErrDimensiones
	}
	pool(K.Filas, trabajadores, filasPorTarea, func(ini, fin int) { filasSpmm(K, U, C, ini, fin) })
	return nil
}
