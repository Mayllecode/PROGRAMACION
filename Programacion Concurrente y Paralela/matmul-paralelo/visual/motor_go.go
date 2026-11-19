// motor_go.go — El motor en Go: el mismo simulador con goroutines.
//
// Usa el mismo worker pool que go/paralelo.go: los nodos se cortan en
// tareas (rangos de filas), p goroutines las sacan de una cola (canal) y un
// WaitGroup espera a que todas terminen. El reparto es DINÁMICO: la goroutine
// que termina antes toma la siguiente tarea. En OpenMP, en cambio, cada hilo
// tiene un bloque FIJO de nodos (schedule(static)).
//
// Cada número se calcula con las mismas operaciones y en el mismo orden que
// en C, así que el resultado es idéntico al Secuencial (el programa lo
// comprueba en vivo).
package main

import (
	"math"
	"sync"
	"time"
)

type tarea struct{ ini, fin int }

// pool: como el de go/paralelo.go, pero cada trabajador sabe su número w
// (para pintar en la vista HILOS qué goroutine calculó cada nodo).
func pool(nFilas, trabajadores, filasPorTarea int, calcular func(w, ini, fin int)) {
	tareas := make(chan tarea)
	var wg sync.WaitGroup
	for w := 0; w < trabajadores; w++ {
		wg.Add(1)
		go func(w int) {
			defer wg.Done()
			for t := range tareas {
				calcular(w, t.ini, t.fin)
			}
		}(w)
	}
	for ini := 0; ini < nFilas; ini += filasPorTarea {
		tareas <- tarea{ini, min(ini+filasPorTarea, nFilas)}
	}
	close(tareas)
	wg.Wait() // barrera: nadie sigue hasta que todos terminaron
}

type MotorGo struct {
	e                             *Escenario
	P                             int // goroutines trabajadoras
	u, up, un, ku, picoU, picoDer []float64
	Duenos                        []int // goroutine que calculó K·U de cada nodo en el último paso
	paso                          int
}

func NuevoMotorGo(e *Escenario, p int) *MotorGo {
	nk := e.N * e.K
	m := &MotorGo{e: e, P: p, u: make([]float64, nk), up: make([]float64, nk), un: make([]float64, nk),
		ku: make([]float64, nk), picoU: make([]float64, nk), picoDer: make([]float64, nk), Duenos: make([]int, e.N)}
	m.Reiniciar()
	return m
}

func (m *MotorGo) U() []float64          { return m.u }
func (m *MotorGo) PicoDeriva() []float64 { return m.picoDer }
func (m *MotorGo) Paso() int             { return m.paso }

func (m *MotorGo) filasPorTarea() int { return max(1, m.e.N/(8*max(1, m.P))) }

// filas de K·U: la misma cuenta que spmm_csr / filasSpmm de go/secuencial.go
func (m *MotorGo) kuFilas(w, ini, fin int) {
	e, k := m.e, m.e.K
	for i := ini; i < fin; i++ {
		m.Duenos[i] = w
		c := m.ku[i*k : (i+1)*k]
		for j := range c {
			c[j] = 0
		}
		for t := e.FilaPtr[i]; t < e.FilaPtr[i+1]; t++ {
			a := e.Val[t]
			b := m.u[e.ColIdx[t]*k : (e.ColIdx[t]+1)*k]
			for j := range c {
				c[j] += float64(a * b[j]) // float64(): sin fusionar (FMA), igual que en C
			}
		}
	}
}

func (m *MotorGo) Reiniciar() {
	e, k := m.e, m.e.K
	for _, s := range [][]float64{m.u, m.up, m.un, m.ku, m.picoU, m.picoDer} {
		clear(s)
	}
	m.paso = 0
	pool(e.N, m.P, m.filasPorTarea(), m.kuFilas)
	dt, a := e.Dt, e.Alpha
	for i := 0; i < e.N; i++ {
		for s := 0; s < k; s++ {
			v := 0.0
			g := e.Ag[s]
			acc := -g - m.ku[i*k+s]/e.Masa[i] - float64(a*v)
			m.up[i*k+s] = m.u[i*k+s] - float64(dt*v) + float64(0.5*dt*dt*acc)
		}
	}
}

func (m *MotorGo) Avanzar(pasos int) float64 {
	e, k := m.e, m.e.K
	dt, a := e.Dt, e.Alpha
	invDt2 := 1.0 / (dt * dt)
	c1, c2 := invDt2+a/(2.0*dt), invDt2-a/(2.0*dt)
	fpt := m.filasPorTarea()
	t0 := time.Now()
	for r := 0; r < pasos && m.paso < e.Pasos; r++ {
		step := m.paso
		u, up, un := m.u, m.up, m.un
		// 1. mover: U(n+1) de cada nodo
		pool(e.N, m.P, fpt, func(_, ini, fin int) {
			for i := ini; i < fin; i++ {
				invM := 1.0 / e.Masa[i]
				for s := 0; s < k; s++ {
					g := e.Ag[step*k+s]
					rhs := float64(2.0*invDt2*u[i*k+s]) - float64(c2*up[i*k+s]) - g - float64(m.ku[i*k+s]*invM)
					un[i*k+s] = rhs / c1
				}
			}
		})
		// 2. picos (después de la barrera: la deriva lee el nodo de abajo)
		pool(e.N, m.P, fpt, func(_, ini, fin int) {
			for i := ini; i < fin; i++ {
				for s := 0; s < k; s++ {
					uu := un[i*k+s]
					d := uu
					if e.Abajo[i] >= 0 {
						d = uu - un[e.Abajo[i]*k+s]
					}
					m.picoU[i*k+s] = math.Max(m.picoU[i*k+s], math.Abs(uu))
					m.picoDer[i*k+s] = math.Max(m.picoDer[i*k+s], math.Abs(d))
				}
			}
		})
		m.up, m.u, m.un = u, un, up // rotar sin copiar
		m.paso++
		// 3. K·U del paso siguiente
		if m.paso < e.Pasos {
			pool(e.N, m.P, fpt, m.kuFilas)
		}
	}
	return time.Since(t0).Seconds()
}
