// main.go — SISMO PARALELO: laboratorio interactivo en la terminal.
//
// Muestra el edificio moviéndose durante el sismo mientras los 3 motores
// (Secuencial en C, OpenMP en C y Go) calculan lo mismo a la vez, y compara
// sus tiempos en vivo ("la carrera"), comprobando que den los mismos números.
//
// Compilar y abrir (desde matmul-paralelo):  VISUAL.bat
package main

import (
	"fmt"
	"math"
	"os"
	"os/signal"
	"strings"
	"time"
)

const (
	cTitulo  = 0xF59E0B
	cBorde   = 0x4B5563
	cTexto   = 0xE5E7EB
	cTenue   = 0x9CA3AF
	cBarraH  = 0x1F2937
	cSeq     = 0xD1D5DB
	cOMP     = 0x60A5FA
	cGo      = 0x22D3EE
	cVerde   = 0x22C55E
	cAmar    = 0xEAB308
	cNaranja = 0xF97316
	cRojo    = 0xEF4444
	cSuelo   = 0xA16207
	cViga    = 0x6B7280
	cColumna = 0x9CA3AF
	cMPI     = 0xF472B6
)

var colorMotor = [4]int32{cSeq, cOMP, cGo, cMPI}

var paleta = []int32{0xF87171, 0x60A5FA, 0x34D399, 0xFBBF24, 0xA78BFA, 0xF472B6, 0x2DD4BF, 0xFB923C,
	0xA3E635, 0x93C5FD, 0xF9A8D4, 0xFDE68A, 0x6EE7B7, 0xC4B5FD, 0xFCA5A5, 0x67E8F9}

type edificio struct {
	pisos, vanos int
	nombre       string
}

var edificios = []edificio{{10, 4, "chico"}, {20, 50, "mediano"}, {50, 200, "grande"}, {100, 400, "muy grande"}}

var velocidades = []int{1, 2, 5, 10, 20}

const nSismos = 8

type App struct {
	iEdif          int
	e              *Escenario
	seq, omp       *MotorC
	gom            *MotorGo
	mpi            *MotorMPI
	hilos, nucleos int
	sismo          int
	foco           int // motor que se explica y se dibuja: 0 Secuencial, 1 OpenMP, 2 Go, 3 MPI
	vista          int // 0 daño, 1 hilos
	pausa, unPaso  bool
	iVel           int
	ema            [4]float64 // ms por paso de cada motor (promedio móvil)
	dif            [3]float64 // diferencia máxima de OpenMP, Go y MPI contra el Secuencial
	duenosOMP      []int
	histSuelo      []float64
	histTecho      []float64
	ayuda          bool
	fin            int
	med            *Medicion
}

func NuevaApp() *App {
	a := &App{nucleos: Nucleos(), foco: 1, sismo: 1, iVel: 2, ayuda: true, pausa: true}
	a.hilos = min(4, a.nucleos)
	HilosOMP(a.hilos)
	a.cargarEdificio()
	return a
}

func (a *App) cargarEdificio() {
	if a.e != nil {
		a.seq.Liberar()
		a.omp.Liberar()
		a.e.Liberar()
	}
	ed := edificios[a.iEdif]
	a.e = NuevoEscenario(ed.pisos, ed.vanos, nSismos, 30.0)
	a.seq = NuevoMotorC(a.e, false)
	a.omp = NuevoMotorC(a.e, true)
	a.gom = NuevoMotorGo(a.e, a.hilos)
	a.duenosOMP = DuenosOMP(a.e.N)
	a.lanzarMPI()
	a.reiniciarMedidas()
}

// lanzarMPI: (re)lanza los procesos MPI con p = hilos (un proceso por "hilo")
func (a *App) lanzarMPI() {
	if a.mpi != nil {
		a.mpi.Cerrar()
	}
	a.mpi = NuevoMotorMPI(a.e, a.hilos, true)
}

func (a *App) Cerrar() {
	if a.mpi != nil {
		a.mpi.Cerrar()
	}
}

func (a *App) reiniciarMedidas() {
	a.ema = [4]float64{}
	a.dif = [3]float64{}
	a.histSuelo, a.histTecho = nil, nil
	a.fin = 0
}

func (a *App) Reiniciar() {
	a.seq.Reiniciar()
	a.omp.Reiniciar()
	a.gom.Reiniciar()
	a.mpi.Reiniciar()
	a.reiniciarMedidas()
}

func (a *App) ponerHilos(p int) {
	p = max(1, min(p, a.nucleos))
	if p == a.hilos {
		return
	}
	a.hilos = p
	HilosOMP(p)
	a.gom.P = p
	a.duenosOMP = DuenosOMP(a.e.N)
	a.lanzarMPI()   // MPI: otra cantidad de procesos, hay que volver a lanzarlos
	a.Reiniciar()   // y el sismo vuelve a empezar para que los 4 vayan juntos
}

func (a *App) motorFoco() Motor {
	switch a.foco {
	case 0:
		return a.seq
	case 1:
		return a.omp
	case 3:
		return a.mpi
	}
	return a.gom
}

// ---------------- teclas ----------------

func (a *App) Tecla(k int) bool {
	if a.med != nil {
		if !a.med.Corriendo() {
			a.med = nil
		}
		return true
	}
	if a.ayuda {
		a.ayuda = false
		return k != 'q' && k != 'Q' && k != 3
	}
	switch {
	case k == 'q' || k == 'Q' || k == 27 || k == 3:
		return false
	case k == ' ':
		a.pausa = !a.pausa
	case k == 'n' || k == 'N':
		a.pausa, a.unPaso = true, true
	case k >= '1' && k <= '0'+nSismos:
		a.sismo = k - '1'
		a.histSuelo, a.histTecho = nil, nil
	case k == 'e' || k == 'E':
		a.iEdif = (a.iEdif + 1) % len(edificios)
		a.cargarEdificio()
	case k == 'm' || k == 'M':
		a.foco = (a.foco + 1) % 4
	case k == 'v' || k == 'V':
		a.vista = 1 - a.vista
	case k == '+' || k == '=':
		a.ponerHilos(a.hilos + 1)
	case k == '-' || k == '_':
		a.ponerHilos(a.hilos - 1)
	case k == 1077 || k == 1072:
		a.iVel = min(a.iVel+1, len(velocidades)-1)
	case k == 1075 || k == 1080:
		a.iVel = max(a.iVel-1, 0)
	case k == 'b' || k == 'B':
		a.med = NuevaMedicion(a)
		go a.med.Correr(a)
	case k == 'r' || k == 'R':
		a.Reiniciar()
	case k == 'h' || k == 'H' || k == '?':
		a.ayuda = true
	}
	return true
}

// ---------------- simulación ----------------

func maxDif(x, y []float64) float64 {
	d := 0.0
	for i := range x {
		d = math.Max(d, math.Abs(x[i]-y[i]))
	}
	return d
}

func (a *App) desplTecho(U []float64) float64 {
	e, s := a.e, a.sismo
	sum := 0.0
	for c := 0; c <= e.Vanos; c++ {
		sum += U[e.Nodo(e.Pisos, c)*e.K+s]
	}
	return sum / float64(e.Vanos+1)
}

func (a *App) Avanzar() {
	if a.med != nil || a.ayuda {
		return
	}
	if a.fin > 0 {
		a.fin--
		if a.fin == 0 {
			a.Reiniciar()
		}
		return
	}
	n := 0
	if !a.pausa {
		n = velocidades[a.iVel]
	} else if a.unPaso {
		n, a.unPaso = 1, false
	}
	n = min(n, a.e.Pasos-a.seq.Paso())
	if n <= 0 {
		return
	}
	t := [4]float64{a.seq.Avanzar(n), a.omp.Avanzar(n), a.gom.Avanzar(n), a.mpi.Avanzar(n)}
	for i := range t {
		if t[i] <= 0 {
			continue
		}
		ms := t[i] / float64(n) * 1000
		if a.ema[i] == 0 {
			a.ema[i] = ms
		} else {
			a.ema[i] = 0.85*a.ema[i] + 0.15*ms
		}
	}
	a.dif[0] = maxDif(a.seq.U(), a.omp.U())
	a.dif[1] = maxDif(a.seq.U(), a.gom.U())
	if a.mpi.Listo() {
		a.dif[2] = maxDif(a.seq.U(), a.mpi.U())
	}
	paso := a.seq.Paso()
	a.histSuelo = append(a.histSuelo, a.e.Ag[(paso-1)*a.e.K+a.sismo])
	a.histTecho = append(a.histTecho, a.desplTecho(a.seq.U()))
	if len(a.histSuelo) > 300 {
		a.histSuelo, a.histTecho = a.histSuelo[1:], a.histTecho[1:]
	}
	if paso >= a.e.Pasos {
		a.fin = 90 // 3 s mostrando "fin del sismo" y vuelve a empezar
	}
}

// ---------------- dibujo ----------------

func (a *App) Dibujar(l *Lienzo) {
	W, H := l.w, l.h
	if W < 100 || H < 33 {
		msg := fmt.Sprintf("Agranda la ventana (mínimo 100 × 33). Ahora: %d × %d. Tip: maximízala o achica la letra con Ctrl + rueda.", W, H)
		l.Parrafo(2, H/2, W-4, 3, msg, cTitulo)
		return
	}
	a.cabecera(l)
	mainH := H - 2 - 10 - 2
	LW := W * 54 / 100
	a.cajaEdificio(l, 0, 2, LW, mainH)

	RW := W - LW
	hSismo := 8
	a.cajaSismo(l, LW, 2, RW, hSismo)
	a.cajaReparto(l, LW, 2+hSismo, RW, mainH-hSismo)
	a.cajaCarrera(l, 0, 2+mainH, W, 10)
	a.teclas(l, H-2)

	if a.ayuda {
		a.cajaAyuda(l)
	}
	if a.med != nil {
		a.cajaMedicion(l)
	}
}

func (a *App) cabecera(l *Lienzo) {
	W := l.w
	l.Fondo(0, 0, W, cBarraH)
	x := l.Negrita(1, 0, "▲ SISMO PARALELO", cTitulo)
	l.Texto(x+2, 0, "laboratorio interactivo · Secuencial vs OpenMP vs Go vs MPI · Matrices paralelas, UNMSM", cTenue)
	l.Negrita(W-11, 0, "[H] ayuda", cTitulo)

	e := a.e
	estado, cEst := "▶ corriendo", int32(cVerde)
	if a.fin > 0 {
		estado, cEst = "■ FIN DEL SISMO: vuelve a empezar", cTitulo
	} else if a.pausa {
		estado, cEst = "⏸ en pausa", cAmar
	}
	ed := edificios[a.iEdif]
	info := fmt.Sprintf(" Edificio %s %d×%d = %s nodos │ Sismo %d/%d (%.1f Hz) │ %d hilos/procesos de %d núcleos │ t = %4.1f/30 s │ vel. ×%d │ ",
		ed.nombre, e.Pisos, e.Vanos, miles(e.N), a.sismo+1, nSismos, e.Freqs[a.sismo], a.hilos, a.nucleos,
		float64(a.seq.Paso())*e.Dt, velocidades[a.iVel])
	x = l.Texto(0, 1, info, cTexto)
	l.Negrita(x, 1, estado, cEst)
}

// muestras: n valores repartidos entre ini y fin (incluye los extremos)
func muestras(ini, fin, n int) []int {
	total := fin - ini + 1
	if n >= total {
		n = total
	}
	out := make([]int, n)
	for j := range out {
		if n == 1 {
			out[j] = fin
		} else {
			out[j] = ini + int(math.Round(float64(j)*float64(total-1)/float64(n-1)))
		}
	}
	return out
}

func (a *App) colorNodo(i int, U []float64) int32 {
	e, s := a.e, a.sismo
	if a.vista == 1 {
		switch a.foco {
		case 0:
			return cSeq
		case 1:
			return paleta[a.duenosOMP[i]%len(paleta)]
		case 3:
			return paleta[a.mpi.Dueno(i)%len(paleta)]
		}
		return paleta[a.gom.Duenos[i]%len(paleta)]
	}
	d := U[i*e.K+s]
	if e.Abajo[i] >= 0 {
		d -= U[e.Abajo[i]*e.K+s]
	}
	return colorDeriva(math.Abs(d) / e.H)
}

func colorDeriva(r float64) int32 {
	switch {
	case r < 0.0025:
		return cVerde
	case r < 0.005:
		return cAmar
	case r < 0.007:
		return cNaranja
	}
	return cRojo
}

func (a *App) cajaEdificio(l *Lienzo, x0, y0, w, h int) {
	vistaN := "DAÑO"
	if a.vista == 1 {
		vistaN = "HILOS"
	}
	l.Caja(x0, y0, w, h, fmt.Sprintf("EDIFICIO · motor %s · vista %s", nombreMotor[a.foco], vistaN), colorMotor[a.foco])
	e, s := a.e, a.sismo
	U := a.motorFoco().U()
	ix, iy, iw, ih := x0+2, y0+1, w-4, h-2

	const maxShift, etiqueta, cellw = 7, 5, 4
	areaH := ih - 4 // nota arriba, suelo y 2 líneas de leyenda
	nf := min(e.Pisos, areaH/2)
	pisos := muestras(1, e.Pisos, nf)
	maxCols := (iw-etiqueta-2*maxShift-2)/cellw + 1
	nc := min(e.Vanos+1, maxCols, 11)
	cols := muestras(0, e.Vanos, nc)
	anchoEd := (nc - 1) * cellw
	base := ix + etiqueta + maxShift + (iw-etiqueta-2*maxShift-anchoEd)/2

	// escala: el techo llega al borde (maxShift) con el mayor desplazamiento visto
	// hasta ahora (como mínimo, el que daría una deriva de 0.2 % en todos los pisos)
	picoU := 0.002 * e.H * float64(e.Pisos)
	for _, v := range a.histTecho {
		picoU = math.Max(picoU, math.Abs(v))
	}
	escala := float64(maxShift) / picoU
	despl := func(p int) int {
		sum := 0.0
		for _, c := range cols {
			sum += U[e.Nodo(p, c)*e.K+s]
		}
		v := int(math.Round(sum / float64(nc) * escala))
		return max(-maxShift-1, min(maxShift+1, v))
	}
	paso := max(0, a.seq.Paso()-1)
	gs := int(math.Round(e.Ag[paso*e.K+s] / e.Amp))

	nota := fmt.Sprintf("Movimiento exagerado: 1 carácter ≈ %.0f cm", 100/escala)
	if nf < e.Pisos || nc < e.Vanos+1 {
		nota = fmt.Sprintf("1 carácter ≈ %.0f cm · se dibujan %d/%d pisos y %d/%d columnas; se calculan TODOS",
			100/escala, nf, e.Pisos, nc, e.Vanos+1)
	}
	l.Parrafo(ix, iy, iw, 1, nota, cTenue)

	y := iy + 1 + (areaH-2*nf)/2
	for j := len(pisos) - 1; j >= 0; j-- {
		p := pisos[j]
		sh := despl(p)
		l.Texto(ix, y, fmt.Sprintf("P%d", p), cTenue)
		for ci, c := range cols {
			x := base + ci*cellw + sh
			l.Pon(x, y, '■', a.colorNodo(e.Nodo(p, c), U))
			if ci < nc-1 {
				for t := 1; t < cellw; t++ {
					l.Pon(x+t, y, '━', cViga)
				}
			}
		}
		y++
		shAbajo, salto := gs, false
		if j > 0 {
			shAbajo, salto = despl(pisos[j-1]), pisos[j-1] != p-1
		} else {
			salto = p != 1
		}
		for ci := range cols {
			xu, xl := base+ci*cellw+sh, base+ci*cellw+shAbajo
			ch := '┃'
			switch {
			case salto:
				ch = '┆'
			case xu-xl >= 1:
				ch = '╱'
			case xu-xl <= -1:
				ch = '╲'
			}
			l.Pon(int(math.Round(float64(xu+xl)/2)), y, ch, cColumna)
		}
		y++
	}
	for x := max(ix+6, base-maxShift-2+gs); x <= base+anchoEd+maxShift+2+gs; x++ {
		l.Pon(x, y, '▀', cSuelo)
	}
	l.Texto(ix, y, "suelo", cSuelo)

	// leyenda
	ly := iy + ih - 2
	if a.vista == 0 {
		x := ix
		for _, it := range []struct {
			c int32
			t string
		}{{cVerde, " < 0.25 %   "}, {cAmar, " < 0.5 %   "}, {cNaranja, " < 0.7 %   "}, {cRojo, " ≥ 0.7 % (límite E.030)"}} {
			l.Pon(x, ly, '■', it.c)
			x = l.Texto(x+1, ly, it.t, cTexto)
		}
		l.Parrafo(ix, ly+1, iw, 1, "Color = deriva: cuánto se inclina cada piso respecto al de abajo.", cTenue)
	} else {
		t := "Color = el hilo que calcula ese nodo."
		switch a.foco {
		case 0:
			t = "Secuencial: un solo hilo calcula todos los nodos (todo del mismo color)."
		case 1:
			t += " OpenMP: bloques fijos de nodos (pisos seguidos)."
		case 2:
			t += " Go: reparto dinámico, cambia en cada paso."
		case 3:
			t = "Color = el PROCESO que calcula ese nodo. MPI: cada proceso tiene una franja de pisos en su propia memoria."
		}
		l.Parrafo(ix, ly, iw, 2, t+" Con [M] cambias de motor.", cTexto)
	}
}

func chispa(v []float64, ancho int, A float64) string {
	const niveles = "▁▂▃▄▅▆▇█"
	nv := []rune(niveles)
	if len(v) > ancho {
		v = v[len(v)-ancho:]
	}
	var b strings.Builder
	for _, x := range v {
		i := int(math.Round((x/A + 1) / 2 * 7))
		b.WriteRune(nv[max(0, min(7, i))])
	}
	return b.String()
}

func (a *App) cajaSismo(l *Lienzo, x0, y0, w, h int) {
	l.Caja(x0, y0, w, h, "EL SISMO", cSuelo)
	e, s := a.e, a.sismo
	ix, iy, iw := x0+2, y0+1, w-4
	l.Texto(ix, iy, fmt.Sprintf("Sismo %d de %d: %.1f Hz, 0.3 g (sintético) · teclas 1 a 8", s+1, nSismos, e.Freqs[s]), cTexto)
	f1 := 1 / e.T1
	x := l.Texto(ix, iy+1, fmt.Sprintf("El edificio vibra solo a %.2f Hz", f1), cTexto)
	if r := e.Freqs[s] / f1; r > 0.8 && r < 1.25 {
		l.Negrita(x, iy+1, " → ¡RESONANCIA!", cRojo)
	}
	ancho := iw - 7
	l.Texto(ix, iy+2, "suelo", cSuelo)
	l.Texto(ix+7, iy+2, chispa(a.histSuelo, ancho, e.Amp), cSuelo)
	A := 0.0
	for _, v := range a.histTecho {
		A = math.Max(A, math.Abs(v))
	}
	l.Texto(ix, iy+3, "techo", cGo)
	l.Texto(ix+7, iy+3, chispa(a.histTecho, ancho, math.Max(A, 1e-6)), cGo)

	pico := a.seq.PicoDeriva()
	dmax := 0.0
	for i := 0; i < e.N; i++ {
		dmax = math.Max(dmax, pico[i*e.K+s])
	}
	dmax /= e.H
	x = l.Texto(ix, iy+4, "Deriva máxima hasta ahora: ", cTexto)
	x = l.Negrita(x, iy+4, fmt.Sprintf("%.2f %%", dmax*100), colorDeriva(dmax))
	l.Texto(x, iy+4, "  (límite E.030: 0.7 %)", cTenue)

	frac := float64(a.seq.Paso()) / float64(e.Pasos)
	nb := iw - 16
	lleno := int(frac * float64(nb))
	x = l.Texto(ix, iy+5, fmt.Sprintf("t %4.1f s ", float64(a.seq.Paso())*e.Dt), cTexto)
	x = l.Texto(x, iy+5, strings.Repeat("█", lleno), cSuelo)
	x = l.Texto(x, iy+5, strings.Repeat("░", nb-lleno), cBorde)
	l.Texto(x, iy+5, " 30 s", cTexto)
}

func (a *App) cajaReparto(l *Lienzo, x0, y0, w, h int) {
	titulos := [4]string{"REPARTO DEL TRABAJO · Secuencial", "REPARTO DEL TRABAJO · OpenMP: bloques fijos",
		"REPARTO DEL TRABAJO · Go: cola de tareas", "REPARTO DEL TRABAJO · MPI: franjas de pisos"}
	l.Caja(x0, y0, w, h, titulos[a.foco], colorMotor[a.foco])
	ix, iy, iw, ih := x0+2, y0+1, w-4, h-2
	e := a.e
	if a.foco == 0 {
		l.Parrafo(ix, iy, iw, ih, fmt.Sprintf("1 hilo calcula los %s nodos, uno tras otro. Pulsa [M] para ver cómo lo reparten OpenMP y Go.", miles(e.N)), cTexto)
		return
	}
	W := e.Vanos + 1
	var items []string
	if a.foco == 3 {
		if !a.mpi.Listo() {
			l.Parrafo(ix, iy, iw, ih, "MPI no disponible: "+a.mpi.Error, cRojo)
			return
		}
		l.Parrafo(ix, iy, iw, 2, fmt.Sprintf("%d procesos, cada uno con SU memoria. En cada paso cada uno manda sus nodos de frontera a sus vecinos.", a.mpi.P), cTenue)
		iy, ih = iy+2, ih-2
		for r := 0; r < a.mpi.P; r++ {
			ini, fin := a.mpi.Starts[r], a.mpi.Starts[r+1]-1
			if fin < ini {
				items = append(items, fmt.Sprintf("P%d (sin nodos)", r))
				continue
			}
			items = append(items, fmt.Sprintf("P%d pisos %d–%d · %d fantasmas", r, ini/W+1, fin/W+1, a.mpi.Fantasmas[r]))
		}
	} else if a.foco == 1 {
		ini := 0
		for i := 1; i <= e.N; i++ {
			if i == e.N || a.duenosOMP[i] != a.duenosOMP[ini] {
				items = append(items, fmt.Sprintf("T%d nodos %s–%s (P%d–P%d)", a.duenosOMP[ini], miles(ini), miles(i-1), ini/W+1, (i-1)/W+1))
				ini = i
			}
		}
	} else {
		cuenta := make([]int, a.hilos)
		for _, d := range a.gom.Duenos {
			if d < len(cuenta) {
				cuenta[d]++
			}
		}
		for g, c := range cuenta {
			items = append(items, fmt.Sprintf("G%d: %s nodos", g, miles(c)))
		}
	}
	colW := iw / 2
	for n, it := range items {
		fila, col := n%ih, n/ih
		if len(items) > 2*ih && n == 2*ih-1 {
			l.Texto(ix+col*colW+2, iy+fila, fmt.Sprintf("… y %d más", len(items)-n), cTenue)
			break
		}
		l.Pon(ix+col*colW, iy+fila, '■', paleta[n%len(paleta)])
		l.Texto(ix+col*colW+2, iy+fila, it, cTexto)
	}
}

func (a *App) cajaCarrera(l *Lienzo, x0, y0, w, h int) {
	l.Caja(x0, y0, w, h, "LA CARRERA · los 4 motores calculan el MISMO edificio y los MISMOS 8 sismos, a la vez", cTitulo)
	ix, iy, iw := x0+2, y0+1, w-4
	const cNom, cHil, cMs, cSp, cVer = 17, 15, 12, 10, 22
	bw := iw - cNom - cHil - cMs - cSp - cVer - 2
	l.Texto(ix, iy, "MOTOR", cTenue)
	l.Texto(ix+cNom, iy, "HILOS", cTenue)
	l.Texto(ix+cNom+cHil, iy, "TIEMPO POR PASO (barra más corta = más rápido)", cTenue)
	l.Texto(ix+cNom+cHil+bw+1, iy, "ms/paso", cTenue)
	l.Texto(ix+cNom+cHil+bw+1+cMs, iy, "SPEEDUP", cTenue)
	l.Texto(ix+cNom+cHil+bw+1+cMs+cSp, iy, "¿MISMO RESULTADO?", cTenue)
	tmax := math.Max(math.Max(a.ema[0], a.ema[1]), math.Max(a.ema[2], a.ema[3]))
	hilos := [4]string{"1 hilo", fmt.Sprintf("%d hilos", a.hilos), fmt.Sprintf("%d goroutines", a.hilos), fmt.Sprintf("%d procesos", a.hilos)}
	if a.hilos == 1 {
		hilos[1], hilos[2], hilos[3] = "1 hilo", "1 goroutine", "1 proceso"
	}
	for m := 0; m < 4; m++ {
		y := iy + 1 + m
		marca := "  "
		if m == a.foco {
			marca = "▶ "
		}
		l.Negrita(ix, y, marca+nombreMotor[m], colorMotor[m])
		l.Texto(ix+cNom, y, hilos[m], cTexto)
		if m == 3 && !a.mpi.Listo() {
			l.Texto(ix+cNom+cHil, y, "no disponible: "+a.mpi.Error, cRojo)
			continue
		}
		if tmax > 0 && a.ema[m] > 0 {
			n := max(1, int(a.ema[m]/tmax*float64(bw)))
			l.Texto(ix+cNom+cHil, y, strings.Repeat("█", n), colorMotor[m])
			l.Texto(ix+cNom+cHil+bw+1, y, fmt.Sprintf("%.4f", a.ema[m]), cTexto)
			sp := a.ema[0] / a.ema[m]
			c := int32(cAmar)
			if sp > 1.05 {
				c = cVerde
			} else if sp < 0.95 {
				c = cRojo
			}
			l.Negrita(ix+cNom+cHil+bw+1+cMs, y, fmt.Sprintf("%.2f×", sp), c)
		}
		xv := ix + cNom + cHil + bw + 1 + cMs + cSp
		if m == 0 {
			l.Texto(xv, y, "es la referencia", cTenue)
		} else if tmax > 0 && a.ema[m] > 0 {
			d := a.dif[m-1]
			switch {
			case d == 0:
				l.Negrita(xv, y, "✓ idéntico", cVerde)
			case d < 1e-12:
				l.Negrita(xv, y, fmt.Sprintf("✓ igual (dif %.0e)", d), cVerde)
			default:
				l.Negrita(xv, y, fmt.Sprintf("✗ dif %.1e", d), cRojo)
			}
		}
	}
	l.Parrafo(ix, iy+5, iw, 2, "→ "+conclusion(a), cTitulo)
	l.Parrafo(ix, iy+7, iw, 1, "Speedup = tiempo del Secuencial ÷ tiempo del motor (2.0× = tarda la mitad). ✓ idéntico = da exactamente los mismos números.", cTenue)
}

func (a *App) teclas(l *Lienzo, y int) {
	filas := [][][2]string{
		{{"Espacio", "pausa"}, {"N", "un paso"}, {"1-8", "sismo"}, {"E", "edificio"}, {"M", "motor que se explica"}, {"V", "vista daño/hilos"}},
		{{"+/-", "hilos y procesos"}, {"←/→", "velocidad"}, {"B", "medir en serio"}, {"R", "reiniciar"}, {"H", "ayuda"}, {"Q", "salir"}},
	}
	for j, f := range filas {
		x := 1
		for _, t := range f {
			x = l.Negrita(x, y+j, "["+t[0]+"]", cTitulo)
			x = l.Texto(x+1, y+j, t[1], cTenue) + 3
		}
	}
}

func (a *App) cajaFlotante(l *Lienzo, w, h int, titulo string, color int32) (int, int, int, int) {
	w, h = min(w, l.w-4), min(h, l.h-2)
	x0, y0 := (l.w-w)/2, (l.h-h)/2
	for y := y0; y < y0+h; y++ {
		for x := x0; x < x0+w; x++ {
			if x >= 0 && x < l.w && y >= 0 && y < l.h {
				l.c[y*l.w+x] = celda{' ', sinColor, 0x111827, false}
			}
		}
	}
	l.Caja(x0, y0, w, h, titulo, color)
	return x0 + 3, y0 + 2, w - 6, h - 3
}

func (a *App) cajaAyuda(l *Lienzo) {
	ix, iy, iw, ih := a.cajaFlotante(l, 100, 17, "BIENVENIDO · SISMO PARALELO", cTitulo)
	y := iy
	for _, ln := range Envolver(textoAyuda, iw) {
		if y >= iy+ih {
			break
		}
		c := int32(cTexto)
		if ln != "" && ln == strings.ToUpper(ln) || strings.HasPrefix(ln, "¿") {
			c = cTitulo
		}
		l.Texto(ix, y, ln, c)
		y++
	}
}

func (a *App) cajaMedicion(l *Lienzo) {
	m := a.med
	m.mu.Lock()
	defer m.mu.Unlock()
	e := a.e
	ed := edificios[a.iEdif]
	ix, iy, iw, _ := a.cajaFlotante(l, 100, 12+3*len(m.hilos)+5, "MEDICIÓN EN SERIO · tecla B", cTitulo)
	l.Texto(ix, iy, fmt.Sprintf("Edificio %s (%s nodos) · 8 sismos · %d pasos · mediana de 3 corridas", ed.nombre, miles(e.N), m.pasos), cTexto)
	if !m.hecho {
		nb := iw - 10
		lleno := m.listo * nb / m.total
		l.Texto(ix, iy+2, "Midiendo: "+m.estado+"…", cTitulo)
		x := l.Texto(ix, iy+3, strings.Repeat("█", lleno), cTitulo)
		l.Texto(x, iy+3, strings.Repeat("░", nb-lleno), cBorde)
		l.Texto(ix, iy+5, "Espera unos segundos: cada motor corre 4 veces la misma simulación (MPI lanza sus procesos cada vez).", cTenue)
		return
	}
	l.Texto(ix, iy+1, fmt.Sprintf("Secuencial (C): T(1) = %.4f s  ← la referencia", m.T1), cSeq)
	maxS := float64(m.hilos[len(m.hilos)-1])
	for h := range m.hilos {
		maxS = math.Max(maxS, math.Max(m.T1/m.tOMP[h], m.T1/m.tGo[h]))
		if m.tMPI[h] > 0 {
			maxS = math.Max(maxS, m.T1/m.tMPI[h])
		}
	}
	bw := iw - 42
	y := iy + 3
	for h, p := range m.hilos {
		for k, t := range []float64{m.tOMP[h], m.tGo[h], m.tMPI[h]} {
			nom, c := "OpenMP", int32(cOMP)
			if k == 1 {
				nom, c = "Go", cGo
			} else if k == 2 {
				nom, c = "MPI", cMPI
			}
			if t <= 0 {
				if k == 0 {
					l.Negrita(ix, y, fmt.Sprintf("%d hilo(s)", p), cTexto)
				}
				l.Texto(ix+11, y, nom, c)
				l.Texto(ix+19, y, "no disponible", cRojo)
				y++
				continue
			}
			if k == 0 {
				l.Negrita(ix, y, fmt.Sprintf("%d hilo(s)", p), cTexto)
			}
			l.Texto(ix+11, y, nom, c)
			sp := m.T1 / t
			n := max(1, int(sp/maxS*float64(bw)))
			l.Texto(ix+19, y, strings.Repeat("█", n), c)
			l.Pon(ix+19+int(float64(p)/maxS*float64(bw))-1, y, '┃', cTitulo)
			cs := int32(cVerde)
			if sp < 0.95 {
				cs = cRojo
			}
			l.Negrita(ix+19+bw+2, y, fmt.Sprintf("%.2f×", sp), cs)
			l.Texto(ix+19+bw+10, y, fmt.Sprintf("(%.4f s)", t), cTenue)
			y++
		}
	}
	y++
	y += l.Parrafo(ix, y, iw, 6, textoMedicion, cTexto)
	l.Texto(ix, y+1, "Pulsa cualquier tecla para volver.", cTitulo)
}

// prueba: sin pantalla, comprueba que OpenMP y Go den lo mismo que el Secuencial
func prueba() {
	HilosOMP(4)
	for _, ed := range edificios {
		e := NuevoEscenario(ed.pisos, ed.vanos, nSismos, 30.0)
		s, o, g := NuevoMotorC(e, false), NuevoMotorC(e, true), NuevoMotorGo(e, 4)
		mp := NuevoMotorMPI(e, 4, false)
		pasos := min(400, e.Pasos)
		ts, to, tg, tm := s.Avanzar(pasos), o.Avanzar(pasos), g.Avanzar(pasos), mp.Avanzar(pasos)
		difM := "MPI no disponible: " + mp.Error
		if mp.Listo() {
			difM = fmt.Sprintf("dif MPI %.1e  mpi(4) %.4f s", maxDif(s.U(), mp.U()), tm)
		}
		fmt.Printf("%-10s %6d nodos  %d pasos  dif OpenMP %.1e  dif Go %.1e  |  seq %.4f s  omp(4) %.4f s  go(4) %.4f s  |  %s\n",
			ed.nombre, e.N, pasos, maxDif(s.U(), o.U()), maxDif(s.U(), g.U()), ts, to, tg, difM)
		mp.Cerrar()
		s.Liberar()
		o.Liberar()
		e.Liberar()
	}
}

func main() {
	if len(os.Args) > 1 && os.Args[1] == "-prueba" {
		prueba()
		return
	}
	if len(os.Args) > 1 && os.Args[1] == "-tabla" {
		// -tabla: la medición de la tecla B para los 4 edificios, en texto (CSV)
		a := NuevaApp()
		defer a.Cerrar()
		fmt.Println("edificio,nodos,pasos,hilos,t_seq_s,t_omp_s,t_go_s,t_mpi_s,speedup_omp,speedup_go,speedup_mpi")
		for i := range edificios {
			a.iEdif = i
			a.cargarEdificio()
			m := NuevaMedicion(a)
			m.Correr(a)
			for h, p := range m.hilos {
				fmt.Printf("%s,%d,%d,%d,%.5f,%.5f,%.5f,%.2f,%.2f\n", edificios[i].nombre, a.e.N, m.pasos, p,
					m.T1, m.tOMP[h], m.tGo[h], m.T1/m.tOMP[h], m.T1/m.tGo[h])
			}
		}
		return
	}
	if len(os.Args) > 1 && os.Args[1] == "-captura" {
		// -captura ancho alto cuadros edificio foco vista hilos sismo [ayuda|medir]: un cuadro sin teclado
		var w, h, cuadros, ed, foco, vista, hilos, sismo int
		fmt.Sscan(strings.Join(os.Args[2:10], " "), &w, &h, &cuadros, &ed, &foco, &vista, &hilos, &sismo)
		a := NuevaApp()
		defer a.Cerrar()
		a.ayuda, a.pausa = false, false
		a.iEdif = ed
		a.cargarEdificio()
		a.ponerHilos(hilos)
		a.foco, a.vista, a.sismo = foco, vista, sismo-1
		for i := 0; i < cuadros; i++ {
			a.Avanzar()
		}
		extra := ""
		if len(os.Args) > 10 {
			extra = os.Args[10]
		}
		if extra == "ayuda" {
			a.ayuda = true
		}
		if extra == "medir" {
			a.med = NuevaMedicion(a)
			a.med.Correr(a)
		}
		l := NuevoLienzo(w, h)
		a.Dibujar(l)
		os.Stdout.WriteString(l.Render())
		return
	}
	ConIniciar()
	os.Stdout.WriteString("\x1b[?1049h\x1b[?25l\x1b[2J")
	salir := func() {
		os.Stdout.WriteString("\x1b[0m\x1b[?25h\x1b[?1049l")
		ConRestaurar()
	}
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt)
	go func() {
		<-sig
		salir()
		os.Exit(0)
	}()
	defer salir()

	a := NuevaApp()
	defer a.Cerrar()
	lw, lh := 0, 0
	for {
		t0 := time.Now()
		for k := ConTecla(); k >= 0; k = ConTecla() {
			if !a.Tecla(k) {
				return
			}
		}
		a.Avanzar()
		w, h := ConTamano()
		if w != lw || h != lh {
			os.Stdout.WriteString("\x1b[2J")
			lw, lh = w, h
		}
		l := NuevoLienzo(w, h)
		a.Dibujar(l)
		os.Stdout.WriteString(l.Render())
		if d := 33*time.Millisecond - time.Since(t0); d > 0 {
			time.Sleep(d)
		}
	}
}
