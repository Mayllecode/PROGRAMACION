// motor_c.go — Puente entre Go y los motores en C (Secuencial y OpenMP).
//
// El programa visual está escrito en Go, pero los motores Secuencial y
// OpenMP son el código C del proyecto, compilado aquí mismo con cgo.
package main

/*
#cgo CFLAGS: -O2 -std=c11 -fopenmp -I../include -D_POSIX_C_SOURCE=200809L
#cgo LDFLAGS: -fopenmp -lm
#include "motor_c.h"
#include "consola.h"
*/
import "C"

import "unsafe"

// Escenario: el edificio (matriz K en CSR, masas) y los k sismos. Es el
// MISMO para los 3 motores: los tres resuelven exactamente el mismo problema.
type Escenario struct {
	c                      *C.Esc
	Pisos, Vanos, N, K     int
	Pasos                  int
	Dt, Alpha, T1, H, Amp  float64
	Freqs                  []float64
	FilaPtr, ColIdx, Abajo []int
	Val, Masa, Ag          []float64
}

func NuevoEscenario(pisos, vanos, k int, duracion float64) *Escenario {
	c := C.esc_crear(C.int(pisos), C.int(vanos), C.int(k), C.double(duracion))
	e := &Escenario{c: c, Pisos: pisos, Vanos: vanos, N: int(c.n), K: k, Pasos: int(c.nsteps),
		Dt: float64(c.dt), Alpha: float64(c.alpha), T1: float64(c.T1), H: float64(c.h), Amp: float64(c.amp)}
	K := c.b.K
	nnz := int(K.nnz)
	e.FilaPtr = enteros(unsafe.Slice(K.row_ptr, e.N+1))
	e.ColIdx = enteros(unsafe.Slice(K.col_idx, nnz))
	e.Abajo = enteros(unsafe.Slice(c.b.below, e.N))
	e.Val = reales(unsafe.Slice(K.val, nnz))
	e.Masa = reales(unsafe.Slice(c.b.mass, e.N))
	e.Ag = reales(unsafe.Slice(c.ag.data, e.Pasos*k))
	e.Freqs = reales(unsafe.Slice(c.freqs, k))
	return e
}

func (e *Escenario) Liberar() { C.esc_liberar(e.c) }

// Nodo: índice del nodo (piso 1..Pisos, columna 0..Vanos), como building_node
func (e *Escenario) Nodo(piso, col int) int { return (piso-1)*(e.Vanos+1) + col }

func enteros(s []C.int) []int {
	r := make([]int, len(s))
	for i, v := range s {
		r[i] = int(v)
	}
	return r
}

func reales(s []C.double) []float64 {
	r := make([]float64, len(s))
	for i, v := range s {
		r[i] = float64(v)
	}
	return r
}

// Motor: lo que los 3 motores saben hacer
type Motor interface {
	Reiniciar()
	Avanzar(pasos int) float64 // segundos que tardó
	U() []float64              // desplazamientos actuales, N x K
	PicoDeriva() []float64     // deriva máxima hasta ahora, N x K
	Paso() int
}

type MotorC struct {
	c *C.MotorC
	e *Escenario
}

func NuevoMotorC(e *Escenario, paralelo bool) *MotorC {
	p := 0
	if paralelo {
		p = 1
	}
	return &MotorC{c: C.mc_crear(e.c, C.int(p)), e: e}
}

func (m *MotorC) Reiniciar()                { C.mc_reiniciar(m.c) }
func (m *MotorC) Avanzar(pasos int) float64 { return float64(C.mc_avanzar(m.c, C.int(pasos))) }
func (m *MotorC) Paso() int                 { return int(m.c.paso) }
func (m *MotorC) Liberar()                  { C.mc_liberar(m.c) }

func (m *MotorC) U() []float64 {
	return unsafe.Slice((*float64)(unsafe.Pointer(m.c.U.data)), m.e.N*m.e.K)
}

func (m *MotorC) PicoDeriva() []float64 {
	return unsafe.Slice((*float64)(unsafe.Pointer(m.c.peak_drift.data)), m.e.N*m.e.K)
}

func HilosOMP(p int) { C.omp_hilos(C.int(p)) }
func Nucleos() int   { return int(C.nucleos()) }

// DuenosOMP: qué hilo de OpenMP calcula cada nodo (schedule(static))
func DuenosOMP(n int) []int {
	out := make([]C.int, n)
	C.duenos_omp(C.int(n), &out[0])
	return enteros(out)
}

func ConIniciar()   { C.con_iniciar() }
func ConRestaurar() { C.con_restaurar() }
func ConTecla() int { return int(C.con_tecla()) }

func ConTamano() (int, int) {
	var w, h C.int
	C.con_tamano(&w, &h)
	return int(w), int(h)
}
