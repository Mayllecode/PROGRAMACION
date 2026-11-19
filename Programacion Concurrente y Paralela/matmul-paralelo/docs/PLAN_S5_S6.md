# Plan de trabajo — semanas 5 y 6 (Fase 1: Fundamentos y modelo)

Escrito para 4 integrantes (A, B, C, D); si son otro número, se reparten
las filas. Lo marcado con ✅ ya está implementado en el repositorio: el
trabajo del equipo es **entenderlo, ejecutarlo, ajustarlo y documentarlo**,
porque en la exposición cualquiera puede ser preguntado por cualquier parte.
Lo marcado con 🟡 está avanzado y le falta la parte que solo puede hacer el
equipo (indicada en la columna "Listo cuando").

## Semana 5 — Definir y preparar

| # | Tarea | Responsable | Cómo | Listo cuando |
|---|---|---|---|---|
| 1 | Repositorio en GitHub | A | Crear el repo, subir este código, agregar a todos como colaboradores, proteger `main` (solo entra por Pull Request) | Todos pueden clonar y hacer push a una rama |
| 2 | Entorno de cada integrante 🟡 | Todos | `docs/INSTALACION.md` (Windows nativo con WinLibs, o WSL2 con `sudo apt install build-essential git openmpi-bin libopenmpi-dev golang-go python3-matplotlib`) → `make env` → subir `results/entorno_<nombre>.txt` | Cada uno tiene OpenMP en [OK] (MPI para S9). Hecho en DESKTOP-R3PLOD9 (`results/entorno.txt`) |
| 3 | Lectura de artículos 🟡 | B (Huang & Chow), C (Quintin et al.), D (Herault et al.) | Leer con las preguntas de `docs/GUIA_LECTURA.md`; partir del borrador de `docs/fichas/` | Cada responsable leyó su artículo y completó/corrigió su ficha (la de Huang & Chow necesita el PDF de IEEE) |
| 4 | Ficha del modelo | A ✅ | Revisar `docs/MODELO_FISICO.md`, ajustar parámetros si un docente o estudiante de civil lo sugiere | El equipo puede explicar la ecuación y por qué el método es explícito |
| 5 | Conseguir acelerogramas reales | D | Buscar registros de sismos peruanos (p. ej. red del CISMID-UNI), anotar fuente, Δt y unidades. El lector ya acepta cabeceras y varias columnas (`data/README.md`) | Al menos 1 registro en `data/` con su fuente en `data/README.md` |
| 6 | Máquina oficial de medición | A | Elegir la PC con más núcleos disponible para todo el semestre (candidata: DESKTOP-R3PLOD9, 6 núcleos / 12 hilos + GPU NVIDIA) | Su `entorno.txt` guardado como `entorno_oficial.txt` |
| 7 | Reunión de cierre de semana | Todos | 45 min: cada uno expone su ficha (10 min) | Decisiones anotadas en `docs/actas.md` (ya tiene la plantilla) |

## Semana 6 — Referencia verificada (Hito H1)

| # | Tarea | Responsable | Cómo | Listo cuando |
|---|---|---|---|---|
| 1 | Producto de matrices secuencial ✅ | B | Leer `src/seq.c`, correr `make test`, explicarlo al resto | 47 pruebas OK en todas las máquinas |
| 2 | Simulador secuencial ✅ | C | Leer `src/sim.c` y `src/building.c`, correr `make test` y `make demo` | 23 pruebas OK; puede explicar la validación con 1 GDL |
| 3 | Registro real en la demo | D | `./build/demo_sismo 10 4 data/registro.txt <dt> <factor> [columna]` | Respuesta del edificio ante un sismo peruano real |
| 4 | Línea base T(1) ✅ | A | `make bench` mide el producto de matrices **y** el simulador (10×4, 20×50, 50×200, 100×400) | CSV en `results/` y tabla con medianas en `docs/HITO_H1.md`. Repetir en la máquina oficial |
| 5 | Gráficos ✅ | B | `make figuras` (`scripts/graficos.py`): validación, convergencia, respuesta por piso, historia del techo, mapa de demanda, T(1) | 7 figuras en `results/figuras/` |
| 6 | Sección "Modelo y verificación" del informe 🟡 | C | 2–3 páginas a partir de `MODELO_FISICO.md` y `HITO_H1.md`: modelo, método, tabla de validación, límites | Borrador revisado por otro integrante |
| 7 | Revisión de código cruzada | Todos | Cada uno revisa el Pull Request de otro | Nadie hizo merge de su propio código |

**Hito H1 (fin de semana 6):** `make test` da 70 pruebas OK en la máquina
oficial, hay una tabla de T(1) y el simulador reproduce el oscilador de 1 GDL
con periodo T = 2π√(m/k). Evidencia: `docs/HITO_H1.md` (cumplido en
DESKTOP-R3PLOD9 el 2026-10-01).

## Qué se prepara ya para la semana 7

Medido con `make bench-sim` (`docs/HITO_H1.md` §4): en edificios grandes el
producto K·U ocupa solo el **~46–48 %** del tiempo y la actualización de U con
el cálculo de picos el **~52–54 %** restante. En la semana 7, OpenMP tiene que
paralelizar **las dos partes**: si solo se paraleliza K·U, la ley de Amdahl
limita el speedup a **~1.9×**, por muchos hilos que haya.
