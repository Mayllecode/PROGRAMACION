/*
 * ground_motion.h — Aceleración del suelo a_g(t) para los escenarios.
 *
 * Dos fuentes:
 *  1. Sintética: senoide con envolvente de subida, meseta y decaimiento.
 *     Sirve para desarrollar y probar sin depender de datos externos.
 *  2. Registro real (acelerograma) leído de un archivo de texto y
 *     re-muestreado al paso de tiempo de la simulación.
 * Todas las salidas están en m/s^2.
 */
#ifndef GROUND_MOTION_H
#define GROUND_MOTION_H

#define G_ACCEL 9.81

/* Escribe nsteps valores en out[0 .. nsteps-1], con out[t] = a_g(t * dt).
 * amp: aceleración pico [m/s^2]; freq: frecuencia dominante [Hz]. */
void ground_motion_synthetic(double *out, int nsteps, double dt, double amp, double freq);

/* Lo que se encontró al leer un registro (para mostrarlo y detectar errores
 * de formato o de unidades). */
typedef struct {
    int samples;       /* muestras leídas del archivo */
    int header_lines;  /* líneas ignoradas: empiezan con '#' o tienen texto */
    int columns;       /* números en la primera línea de datos */
    double pga;        /* aceleración pico del registro, max |a| [m/s^2] */
} RecordInfo;

/* Lee un acelerograma de texto.
 *
 * Una línea es de DATOS si todo lo que contiene son números (separados por
 * espacios, tabs, ',' o ';'; punto decimal; se acepta el exponente Fortran
 * 1.0D-03). Cualquier otra línea (cabecera con texto, o que empieza con '#')
 * se ignora entera: así los números de la cabecera (fecha, coordenadas,
 * n.º de muestras) no se confunden con aceleraciones.
 *
 *   column:       0  -> todos los números de cada línea, en orden
 *                       (una columna, o varios valores por línea como el
 *                       formato PEER .AT2)
 *                 c>=1 -> solo el c-ésimo número de cada línea
 *                       (archivos con tiempo + aceleración, o con las tres
 *                       componentes EW NS UD como los del CISMID)
 *   record_dt:    paso de tiempo del registro [s]
 *   units_factor: multiplica cada valor para pasarlo a m/s^2
 *                 (gal = cm/s^2 -> 0.01; fracción de g -> 9.81; m/s^2 -> 1)
 *
 * Re-muestrea por interpolación lineal a sim_dt y llena out[0 .. nsteps-1]
 * (con 0 después del final del registro). info puede ser NULL.
 *
 * Devuelve el número de muestras leídas; -1 si no se pudo abrir el archivo
 * o los parámetros no son válidos; -2 si no hay datos o la columna pedida
 * no existe en el archivo. */
int ground_motion_load(const char *path, int column, double record_dt, double units_factor,
                       double sim_dt, int nsteps, double *out, RecordInfo *info);

#endif
