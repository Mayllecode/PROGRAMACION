// motor_mpi.go — El 4.º motor: MPI (memoria distribuida, semanas 9 a 11).
//
// OpenMP y Go corren dentro de este programa (hilos que comparten la
// memoria). MPI no puede: son VARIOS PROCESOS, cada uno con su propia
// memoria. Por eso este motor lanza
//
//	mpiexec -n P build\motor_mpi.exe pisos vanos 8 30 puerto
//
// y le da órdenes por una conexión local (127.0.0.1) al proceso 0:
// "avanza n pasos" -> responde el tiempo y el desplazamiento U de TODOS los
// nodos (lo junta de los P procesos). Así el visual dibuja el edificio que
// calculó MPI y comprueba que es idéntico al del Secuencial.
//
// Cada proceso tiene una franja de pisos (reparto por no nulos, como en
// src/mpi/sim_mpi.c); Starts dice de qué nodo a qué nodo va cada una.
package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"time"
)

type MotorMPI struct {
	P         int
	e         *Escenario
	cmd       *exec.Cmd
	conn      net.Conn
	u, cero   []float64
	buf       []byte
	paso      int
	Starts    []int // el proceso r calcula los nodos Starts[r] .. Starts[r+1]-1
	Fantasmas []int // nodos fantasma de cada proceso (los de frontera de sus vecinos)
	Error     string
}

// rutaMPI: mpiexec de MS-MPI y el motor (al lado de sismo_visual.exe, en build\)
func rutaMPI() (string, string) {
	mpiexec := "mpiexec"
	if d := os.Getenv("MSMPI_BIN"); d != "" {
		mpiexec = filepath.Join(d, "mpiexec.exe")
	} else if _, err := os.Stat(`C:\Program Files\Microsoft MPI\Bin\mpiexec.exe`); err == nil {
		mpiexec = `C:\Program Files\Microsoft MPI\Bin\mpiexec.exe`
	}
	motor := "motor_mpi.exe"
	if exe, err := os.Executable(); err == nil {
		motor = filepath.Join(filepath.Dir(exe), "motor_mpi.exe")
	}
	return mpiexec, motor
}

// NuevoMotorMPI lanza P procesos. pasiva = true: los procesos que esperan
// sueltan el núcleo (MSMPI_PROGRESS_SPIN_LIMIT=1), para no robarle CPU a los
// otros 3 motores mientras corren; en la medición (tecla B) va en false.
func NuevoMotorMPI(e *Escenario, p int, pasiva bool) *MotorMPI {
	m := &MotorMPI{P: p, e: e, u: make([]float64, e.N*e.K), cero: make([]float64, e.N*e.K)}
	mpiexec, motor := rutaMPI()
	if _, err := os.Stat(motor); err != nil {
		m.Error = "falta build\\motor_mpi.exe (correr VISUAL.bat o make mpi)"
		return m
	}
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		m.Error = "no se pudo abrir la conexión local"
		return m
	}
	defer ln.Close()
	puerto := ln.Addr().(*net.TCPAddr).Port
	args := []string{"-n", fmt.Sprint(p)}
	if pasiva {
		args = append(args, "-env", "MSMPI_PROGRESS_SPIN_LIMIT", "1")
	}
	args = append(args, motor, fmt.Sprint(e.Pisos), fmt.Sprint(e.Vanos), fmt.Sprint(e.K), "30", fmt.Sprint(puerto))
	m.cmd = exec.Command(mpiexec, args...)
	m.cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000} // sin ventana propia
	if err := m.cmd.Start(); err != nil {
		m.Error = "no se pudo correr mpiexec (¿está instalado MS-MPI?)"
		return m
	}
	ln.(*net.TCPListener).SetDeadline(time.Now().Add(20 * time.Second))
	conn, err := ln.Accept()
	if err != nil {
		m.Error = "los procesos MPI no respondieron"
		m.cmd.Process.Kill()
		m.cmd.Wait()
		return m
	}
	m.conn = conn
	var cab [4]int32 // n, k, P, pasos
	if binary.Read(conn, binary.LittleEndian, &cab) != nil || int(cab[0]) != e.N || int(cab[1]) != e.K {
		m.Error = "el motor MPI armó otro edificio"
		m.Cerrar()
		return m
	}
	rep := make([]int32, 2*p+1)
	binary.Read(conn, binary.LittleEndian, rep)
	m.Starts = make([]int, p+1)
	m.Fantasmas = make([]int, p)
	for r := 0; r <= p; r++ {
		m.Starts[r] = int(rep[r])
	}
	for r := 0; r < p; r++ {
		m.Fantasmas[r] = int(rep[p+1+r])
	}
	m.buf = make([]byte, 8*e.N*e.K)
	return m
}

func (m *MotorMPI) Listo() bool { return m.conn != nil && m.Error == "" }

func (m *MotorMPI) orden(c byte, arg int) error {
	var b [5]byte
	b[0] = c
	binary.LittleEndian.PutUint32(b[1:], uint32(int32(arg)))
	_, err := m.conn.Write(b[:])
	return err
}

func (m *MotorMPI) fallo(err error) {
	if m.Error == "" {
		m.Error = "se cortó la conexión con los procesos MPI: " + err.Error()
	}
}

func (m *MotorMPI) Reiniciar() {
	m.paso = 0
	if !m.Listo() {
		return
	}
	var paso int32
	if err := m.orden('R', 0); err != nil {
		m.fallo(err)
		return
	}
	if err := binary.Read(m.conn, binary.LittleEndian, &paso); err != nil {
		m.fallo(err)
		return
	}
	copy(m.u, m.cero)
}

// Avanzar: los P procesos avanzan n pasos; devuelve lo que tardó el más lento
func (m *MotorMPI) Avanzar(pasos int) float64 {
	if !m.Listo() {
		return 0
	}
	if err := m.orden('A', pasos); err != nil {
		m.fallo(err)
		return 0
	}
	var t float64
	var paso int32
	if err := binary.Read(m.conn, binary.LittleEndian, &t); err != nil {
		m.fallo(err)
		return 0
	}
	if err := binary.Read(m.conn, binary.LittleEndian, &paso); err != nil {
		m.fallo(err)
		return 0
	}
	if _, err := io.ReadFull(m.conn, m.buf); err != nil {
		m.fallo(err)
		return 0
	}
	for i := range m.u {
		m.u[i] = math.Float64frombits(binary.LittleEndian.Uint64(m.buf[8*i:]))
	}
	m.paso = int(paso)
	return t
}

func (m *MotorMPI) U() []float64          { return m.u }
func (m *MotorMPI) PicoDeriva() []float64 { return m.cero } // el visual usa la del Secuencial
func (m *MotorMPI) Paso() int             { return m.paso }

// Dueno: qué proceso calcula el nodo i
func (m *MotorMPI) Dueno(i int) int {
	for r := 0; r+1 < len(m.Starts); r++ {
		if i < m.Starts[r+1] {
			return r
		}
	}
	return 0
}

func (m *MotorMPI) Cerrar() {
	if m.conn != nil {
		m.orden('Q', 0)
		m.conn.Close()
		m.conn = nil
	}
	if m.cmd != nil && m.cmd.Process != nil {
		hecho := make(chan struct{})
		go func() { m.cmd.Wait(); close(hecho) }()
		select {
		case <-hecho:
		case <-time.After(3 * time.Second):
			m.cmd.Process.Kill()
		}
		m.cmd = nil
	}
}
