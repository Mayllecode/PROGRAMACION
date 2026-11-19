# Ficha del modelo físico

Entregable de la semana 5. Describe qué simulamos, con qué supuestos y cómo
sabemos que el simulador es correcto. Es la base de la sección "Modelo" del
informe final.

## 1. Qué simulamos

La respuesta de un edificio ante el movimiento del suelo durante un sismo:
cuánto se desplaza cada punto y cuánto se deforma cada piso respecto al de
abajo (deriva). Varios sismos (escenarios) se simulan a la vez.

## 2. Modelo de la estructura

Pórtico 2D de `pisos × vanos`. Un nodo en cada cruce piso–columna, con un
grado de libertad: su desplazamiento horizontal relativo al suelo.

- **Masa** m concentrada en cada nodo → matriz de masas M **diagonal**.
- **Columnas**: resorte de rigidez kc entre un nodo y el de abajo
  (o el suelo, en el piso 1).
- **Losas y vigas**: resorte de rigidez kb entre nodos vecinos del mismo piso.
- Base empotrada: el suelo no se deforma, solo se acelera.

La matriz de rigidez K resulta **simétrica y dispersa**: cada fila tiene como
máximo 5 no nulos, sin importar el tamaño del edificio.

## 3. Ecuación y método numérico

Ecuación de movimiento, con U de tamaño n × k (una columna por escenario):

    M·Ü + α·M·U' + K·U = −M·1·a_g(t)

Se usa el método de **diferencias centrales**, que es explícito:

    c1·U(n+1) = (2/Δt²)·U(n) − c2·U(n−1) − a_g(n) − M⁻¹·K·U(n)
    c1 = 1/Δt² + α/(2Δt)      c2 = 1/Δt² − α/(2Δt)

Por qué este método:
- M es diagonal: invertirla es dividir. **No hay que resolver sistemas
  lineales**, y el único trabajo pesado es el producto K·U.
- Cada escenario es una columna de U: un solo producto avanza todos los sismos.

Restricción: es estable solo si **Δt < 2/ω_max**. El código calcula una cota
segura de ese límite y usa la mitad.

## 4. Parámetros por defecto (demo)

| Parámetro | Valor | Nota |
|---|---|---|
| Masa por nodo m | 20 000 kg | |
| Rigidez de columna kc | 3.5 × 10⁷ N/m | elegida para que 10 pisos den T1 ≈ 1.0 s |
| Rigidez de losa kb | 1.0 × 10⁸ N/m | losa más rígida que las columnas |
| Altura de entrepiso h | 3.0 m | para la deriva = desplazamiento relativo / h |
| Amortiguamiento | 5 % en el primer modo | α = 2·ζ·ω1 |
| Duración | 30 s | |

La regla práctica de que el periodo de un edificio es aproximadamente
0.1 s × número de pisos sirve para chequear que T1 ≈ 1 s en 10 pisos sea razonable.
Conviene validar estos valores con un docente o estudiante de ingeniería civil.

## 5. Escenarios sísmicos

- **Sintéticos** (ya implementados): senoide con envolvente de subida (2 s),
  fase fuerte (hasta 10 s) y decaimiento. Se varía la frecuencia dominante
  para mostrar la **resonancia**: el sismo de 1 Hz sobre un edificio de
  T1 = 1 s produce desplazamientos unas 6 veces mayores que el de 0.5 Hz.
  Una senoide pura en resonancia es un caso extremo, más severo que un sismo real.
- **Reales** (semana 5–6): acelerogramas de sismos peruanos, por ejemplo de
  la red de acelerógrafos del CISMID-UNI. El programa lee un archivo de
  texto con su Δt y un factor de unidades (gal → 0.01): ignora sola la
  cabecera (toda línea con texto), permite elegir la columna (EW, NS, UD o
  tiempo + aceleración), re-muestrea al Δt de la simulación e informa la PGA
  para detectar errores de unidades. Detalle en `data/README.md`.

## 6. Cómo sabemos que el simulador es correcto (`tests/test_sim.c`)

| Prueba | Resultado esperado | Obtenido |
|---|---|---|
| Oscilador de 1 GDL sin amortiguar | u(t) = cos(ωt) | error 1.2·10⁻⁴ en 5 periodos |
| **Periodo medido (Hito H1)** | T = 2π√(m/k) = 0.314159 s | 0.314156 s, error 1.0·10⁻⁵ |
| Oscilador con amortiguamiento | solución analítica amortiguada | error 3.2·10⁻⁵ |
| Orden de convergencia | error(Δt) / error(Δt/2) ≈ 4 | 4.00 (también en 6 refinamientos, `make validar`) |
| T1 del edificio de 10 pisos | fórmula cerrada del edificio de cortante | 1.004929 s, error < 10⁻¹² |
| Sismo constante + mucho amortiguamiento | equilibrio K·u = −M·1·a | residuo 10⁻¹⁴ |
| Linealidad | resp(A+B) = resp(A) + resp(B) | error 5·10⁻¹⁶ |
| Escenarios juntos vs. separados | idénticos | exacto |
| Estabilidad con 0.95·Δt crítico | respuesta acotada | acotada |
| Lectura de acelerogramas | cabecera ignorada, columna, unidades, re-muestreo | 6 pruebas OK |

Las curvas de esta validación están en `results/figuras/01_validacion_1gdl.png`
y `02_convergencia.png`; el detalle completo, en `docs/HITO_H1.md`.

## 7. Límites del modelo (decirlo en la exposición)

- Lineal y elástico: no representa fisuras, daño ni colapso.
- 2D y un grado de libertad por nodo: no hay torsión ni movimiento vertical.
- No reemplaza un análisis según la Norma E.030 ni el criterio de un ingeniero.
  La E.030 (tabla "Límites para la distorsión del entrepiso") fija 0,007 para
  concreto armado, pero se aplica a derivas de diseño (inelásticas) obtenidas
  con su espectro; la línea de 0,007 en la figura 03 es solo una referencia
  visual. Citar la edición vigente de la norma si se menciona en el informe.

El objetivo del proyecto es el **cómputo paralelo**; el modelo es lo
suficientemente realista para que el problema sea genuino y lo
suficientemente simple para verificarlo.
