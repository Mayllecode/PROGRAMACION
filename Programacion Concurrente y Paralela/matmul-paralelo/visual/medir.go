// medir.go — Tecla [B]: medición "en serio" del edificio actual.
//
// Igual que bench_sim_omp: 400 pasos desde el reposo, mediana de 3 (más una
// corrida de calentamiento), con 1, 2, 4 y 8 hilos, para OpenMP, Go y MPI
// (con MPI: 1, 2, 4 y 8 procesos).
package main

import (
	"fmt"
	"sort"
	"sync"
)

type Medicion struct {
	mu           sync.Mutex
	hecho        bool
	estado       string
	listo, total int
	pasos        int
	T1           float64
	hilos        []int
	tOMP, tGo    []float64
	tMPI         []float64 // 0 si MPI no está disponible
}

func (m *Medicion) Corriendo() bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	return !m.hecho
}

func NuevaMedicion(a *App) *Medicion {
	m := &Medicion{pasos: min(400, a.e.Pasos)}
	for _, p := range []int{1, 2, 4, 8} {
		if p <= a.nucleos {
			m.hilos = append(m.hilos, p)
		}
	}
	m.tOMP = make([]float64, len(m.hilos))
	m.tGo = make([]float64, len(m.hilos))
	m.tMPI = make([]float64, len(m.hilos))
	m.total = 1 + 3*len(m.hilos)
	m.estado = "Secuencial (C)"
	return m
}

func (m *Medicion) avance(estado string) {
	m.mu.Lock()
	m.listo++
	m.estado = estado
	m.mu.Unlock()
}

func (m *Medicion) mediana(motor Motor) float64 {
	motor.Reiniciar()
	motor.Avanzar(m.pasos) // calentamiento
	var t [3]float64
	for r := range t {
		motor.Reiniciar()
		t[r] = motor.Avanzar(m.pasos)
	}
	sort.Float64s(t[:])
	return t[1]
}

// Correr hace toda la medición (en otra goroutine: la pantalla sigue viva)
func (m *Medicion) Correr(a *App) {
	e := a.e
	s := NuevoMotorC(e, false)
	m.T1 = m.mediana(s)
	s.Liberar()
	for h, p := range m.hilos {
		m.avance(fmt.Sprintf("OpenMP con %d hilo(s)", p))
		HilosOMP(p)
		o := NuevoMotorC(e, true)
		m.tOMP[h] = m.mediana(o)
		o.Liberar()
		m.avance(fmt.Sprintf("Go con %d goroutine(s)", p))
		m.tGo[h] = m.mediana(NuevoMotorGo(e, p))
		m.avance(fmt.Sprintf("MPI con %d proceso(s)", p))
		mm := NuevoMotorMPI(e, p, false) // espera activa: aquí MPI corre solo
		if mm.Listo() {
			m.tMPI[h] = m.mediana(mm)
		}
		mm.Cerrar()
	}
	HilosOMP(a.hilos)
	m.mu.Lock()
	m.listo = m.total
	m.hecho = true
	m.mu.Unlock()
}
