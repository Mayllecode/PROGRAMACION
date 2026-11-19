# Acelerogramas (registros sísmicos reales)

Esta carpeta guarda los registros que se usan como escenario real en la demo.
**Cada archivo debe tener su fuente anotada en la tabla de abajo** (estación,
sismo, fecha, componente, Δt, unidades y de dónde se descargó). Un registro sin
fuente no se usa en el informe.

| Archivo | Sismo | Estación | Componente | Δt [s] | Unidades | Fuente / enlace | Quién lo bajó |
|---|---|---|---|---|---|---|---|
| *(pendiente, tarea S5-5)* | | | | | | | |

## Dónde conseguirlos

- **CISMID-UNI**, Red Acelerográfica (REDACIS): registros de sismos peruanos
  (por ejemplo Pisco 2007). Suelen venir con varias líneas de cabecera y tres
  columnas de aceleración (EW, NS, UD), en gal (cm/s²).
- **IGP** (Instituto Geofísico del Perú): catálogo y algunos registros de la
  red acelerográfica.
- **PEER NGA** (requiere registro gratuito): formato `.AT2`, en fracción de g,
  con varios valores por línea.

## Cómo se leen (ya está implementado y probado)

`ground_motion_load` (en `src/ground_motion.c`):

- Una línea es de **datos** solo si todo lo que tiene son números (separados por
  espacios, tabs, `,` o `;`, con **punto decimal**). Cualquier línea con texto,
  o que empieza con `#`, se toma como **cabecera** y se ignora entera: así la
  fecha, las coordenadas o el número de muestras de la cabecera no se confunden
  con aceleraciones.
- Se puede elegir **qué columna** leer (útil con tiempo + aceleración, o con
  EW/NS/UD). Con columna 0 se leen todos los números en orden (una columna, o
  varios valores por línea como el formato PEER).
- Si la cabecera tiene una línea **solo con números** (por ejemplo `18000 0.01`),
  hay que comentarla con `#`: no hay forma de distinguirla de un dato.

```bash
# demo  pisos vanos  archivo            dt_registro  factor_a_m/s2  [columna]
./build/demo_sismo 10 4  data/pisco_ica2.txt  0.01      0.01           1   # gal, columna EW
./build/demo_sismo 10 4  data/registro.AT2    0.005     9.81           0   # fracción de g, PEER
```

| Unidades del archivo | factor |
|---|---|
| gal = cm/s² | `0.01` |
| fracción de g | `9.81` |
| m/s² | `1` |

La demo imprime cuántas muestras leyó, cuántas líneas de cabecera ignoró y la
**PGA** (aceleración pico) del registro en m/s² y en g. Si la PGA sale absurda
(por ejemplo 30 g), el factor de unidades está mal.
