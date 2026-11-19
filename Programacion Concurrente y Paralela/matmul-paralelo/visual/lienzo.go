// lienzo.go — Una "hoja" de caracteres con color que se dibuja entera en
// cada cuadro (como un fotograma) y se manda a la terminal de una vez.
package main

import (
	"fmt"
	"strings"
	"unicode/utf8"
)

const sinColor = -1

type celda struct {
	r      rune
	fg, bg int32
	bold   bool
}

type Lienzo struct {
	w, h int
	c    []celda
}

func NuevoLienzo(w, h int) *Lienzo {
	l := &Lienzo{w: w, h: h, c: make([]celda, w*h)}
	l.Limpiar()
	return l
}

func (l *Lienzo) Limpiar() {
	for i := range l.c {
		l.c[i] = celda{' ', sinColor, sinColor, false}
	}
}

func (l *Lienzo) Pon(x, y int, r rune, fg int32) {
	if x >= 0 && y >= 0 && x < l.w && y < l.h {
		c := &l.c[y*l.w+x]
		c.r, c.fg = r, fg
	}
}

func (l *Lienzo) Fondo(x, y, w int, bg int32) {
	for i := 0; i < w; i++ {
		if x+i >= 0 && x+i < l.w && y >= 0 && y < l.h {
			l.c[y*l.w+x+i].bg = bg
		}
	}
}

// Texto escribe s desde (x, y) y devuelve la x donde terminó
func (l *Lienzo) Texto(x, y int, s string, fg int32) int {
	for _, r := range s {
		l.Pon(x, y, r, fg)
		x++
	}
	return x
}

func (l *Lienzo) Negrita(x, y int, s string, fg int32) int {
	for _, r := range s {
		if x >= 0 && y >= 0 && x < l.w && y < l.h {
			l.c[y*l.w+x] = celda{r, fg, l.c[y*l.w+x].bg, true}
		}
		x++
	}
	return x
}

// Caja con borde redondeado y título
func (l *Lienzo) Caja(x, y, w, h int, titulo string, color int32) {
	if w < 2 || h < 2 {
		return
	}
	for i := 1; i < w-1; i++ {
		l.Pon(x+i, y, '─', color)
		l.Pon(x+i, y+h-1, '─', color)
	}
	for j := 1; j < h-1; j++ {
		l.Pon(x, y+j, '│', color)
		l.Pon(x+w-1, y+j, '│', color)
	}
	l.Pon(x, y, '╭', color)
	l.Pon(x+w-1, y, '╮', color)
	l.Pon(x, y+h-1, '╰', color)
	l.Pon(x+w-1, y+h-1, '╯', color)
	if titulo != "" {
		l.Negrita(x+2, y, " "+titulo+" ", color)
	}
}

// Parrafo: escribe s cortando por palabras dentro de un ancho w.
// Devuelve cuántas líneas usó (sin pasar de maxLineas).
func (l *Lienzo) Parrafo(x, y, w, maxLineas int, s string, fg int32) int {
	lineas := Envolver(s, w)
	n := 0
	for _, ln := range lineas {
		if n >= maxLineas {
			break
		}
		l.Texto(x, y+n, ln, fg)
		n++
	}
	return n
}

func Envolver(s string, w int) []string {
	var out []string
	for _, par := range strings.Split(s, "\n") {
		linea := ""
		for _, pal := range strings.Fields(par) {
			if linea == "" {
				linea = pal
			} else if utf8.RuneCountInString(linea)+1+utf8.RuneCountInString(pal) <= w {
				linea += " " + pal
			} else {
				out = append(out, linea)
				linea = pal
			}
		}
		out = append(out, linea)
	}
	return out
}

func rgb(c int32) (int32, int32, int32) { return (c >> 16) & 255, (c >> 8) & 255, c & 255 }

// Render: todo el cuadro como texto con códigos de color ANSI
func (l *Lienzo) Render() string {
	var b strings.Builder
	b.Grow(l.w * l.h * 4)
	for y := 0; y < l.h; y++ {
		fmt.Fprintf(&b, "\x1b[%d;1H\x1b[0m", y+1)
		fg, bg, bold := int32(sinColor), int32(sinColor), false
		ancho := l.w
		if y == l.h-1 {
			ancho-- // no escribir la última celda: haría bajar la pantalla
		}
		for x := 0; x < ancho; x++ {
			c := l.c[y*l.w+x]
			if c.bold != bold {
				b.WriteString("\x1b[0m")
				fg, bg, bold = sinColor, sinColor, false
				if c.bold {
					b.WriteString("\x1b[1m")
					bold = true
				}
			}
			if c.fg != fg {
				if c.fg == sinColor {
					b.WriteString("\x1b[39m")
				} else {
					r, g, bl := rgb(c.fg)
					fmt.Fprintf(&b, "\x1b[38;2;%d;%d;%dm", r, g, bl)
				}
				fg = c.fg
			}
			if c.bg != bg {
				if c.bg == sinColor {
					b.WriteString("\x1b[49m")
				} else {
					r, g, bl := rgb(c.bg)
					fmt.Fprintf(&b, "\x1b[48;2;%d;%d;%dm", r, g, bl)
				}
				bg = c.bg
			}
			b.WriteRune(c.r)
		}
	}
	b.WriteString("\x1b[0m")
	return b.String()
}
