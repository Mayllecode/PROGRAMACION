// textos.go — Las explicaciones que muestra el programa, en lenguaje simple.
package main

import "fmt"

var nombreMotor = [4]string{"Secuencial (C)", "OpenMP (C)", "Go", "MPI (C)"}

// conclusion: lo que dice la carrera en este momento, en una frase
func conclusion(a *App) string {
	if a.ema[0] == 0 {
		if a.pausa {
			return "En pausa: pulsa [Espacio] para que empiece el sismo (o [N] para avanzar de a un paso)."
		}
		return "Midiendo…"
	}
	sO, sG := a.ema[0]/a.ema[1], a.ema[0]/a.ema[2]
	mejor, s := "OpenMP", sO
	if sG > sO {
		mejor, s = "Go", sG
	}
	if a.ema[3] > 0 {
		if sM := a.ema[0] / a.ema[3]; sM > s {
			mejor, s = "MPI", sM
		}
	}
	switch {
	case a.hilos == 1:
		return "Con 1 hilo no hay nada que repartir: el paralelo solo puede empatar o perder. Sube los hilos con [+]."
	case s < 0.95:
		return fmt.Sprintf("Con %s nodos el paralelo PIERDE: repartir el trabajo y esperar a los hilos cuesta "+
			"más que la cuenta misma. Prueba [E] con un edificio más grande.", miles(a.e.N))
	case s <= 1.05:
		return "EMPATE: lo que se ahorra repartiendo casi se lo come el costo de coordinar los hilos. Prueba un edificio más grande con [E]."
	default:
		quien := "cada hilo calcula"
		if mejor == "MPI" {
			quien = "cada proceso calcula, en su propia memoria,"
		}
		return fmt.Sprintf("GANA %s: %.1f× más rápido que el Secuencial con %d, porque %s "+
			"solo su parte de los %s nodos al mismo tiempo. Mídelo bien con [B].", mejor, s, a.hilos, quien, miles(a.e.N))
	}
}

const textoAyuda = `UN EDIFICIO EN UN SISMO
El edificio es una malla de nodos (■) unidos por vigas y columnas que actúan como resortes. En cada paso se calcula K·U (rigidez × desplazamientos = fuerzas) y se mueve cada nodo. Un sismo de 30 s son unos 4 900 pasos.

4 MOTORES HACEN LA MISMA CUENTA A LA VEZ
• Secuencial (C): 1 núcleo.  • OpenMP (C): varios hilos.  • Go: varias goroutines.
• MPI (C): varios PROCESOS, cada uno con su propia memoria y una franja de pisos.
Abajo, LA CARRERA: quién tarda menos por paso. ✓ = dieron los mismos números.

TECLAS: Espacio pausa · N un paso · 1-8 sismo · E edificio · M motor · V vista · +/- hilos · ←/→ velocidad · B medir · R reiniciar · Q salir

Pulsa cualquier tecla para empezar.`

const textoMedicion = "Speedup = T(Secuencial) ÷ T(paralelo). La marca ┃ es el ideal: con p hilos, p veces más rápido.\n" +
	"¿Por qué no llega al ideal? 1) Amdahl: hay partes que no se reparten. 2) Memoria: todos los hilos leen la RAM a la vez. " +
	"3) Coordinar: crear, repartir y esperar a los hilos cuesta tiempo.\n" +
	"Edificio chico: el punto 3 pesa más que la cuenta → speedup menor que 1. Edificio grande: la cuenta pesa más → speedup mayor que 1."

// miles: 40100 -> "40 100"
func miles(n int) string {
	s := fmt.Sprint(n)
	out := ""
	for i, r := range s {
		if i > 0 && (len(s)-i)%3 == 0 {
			out += " "
		}
		out += string(r)
	}
	return out
}
