#include "ground_motion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"

void ground_motion_synthetic(double *out, int nsteps, double dt, double amp, double freq) {
    const double t_rise = 2.0, t_strong = 10.0, decay = 0.3; /* envolvente típica */
    for (int s = 0; s < nsteps; s++) {
        double t = s * dt, env;
        if (t < t_rise) env = (t / t_rise) * (t / t_rise);
        else if (t < t_strong) env = 1.0;
        else env = exp(-decay * (t - t_strong));
        out[s] = amp * env * sin(2.0 * 3.14159265358979323846 * freq * t);
    }
}

#define LINE_MAX_LEN 8192

/* Convierte una línea en números. Devuelve cuántos leyó (0 = línea vacía)
 * o -1 si contiene algo que no es un número: entonces es cabecera. */
static int parse_numeric_line(char *line, double *vals) {
    int n = 0;
    for (char *tok = strtok(line, " \t\r\n,;"); tok; tok = strtok(NULL, " \t\r\n,;")) {
        for (char *c = tok + 1; *c; c++)          /* exponente Fortran: 1.0D-03 */
            if (*c == 'D' || *c == 'd') *c = 'E';
        char *end;
        double v = strtod(tok, &end);
        if (end == tok || *end != '\0' || !isfinite(v)) return -1;
        vals[n++] = v;
    }
    return n;
}

int ground_motion_load(const char *path, int column, double record_dt, double units_factor,
                       double sim_dt, int nsteps, double *out, RecordInfo *info) {
    RecordInfo ri = {0, 0, 0, 0.0};
    if (info) *info = ri;
    if (column < 0 || record_dt <= 0.0 || sim_dt <= 0.0) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    size_t cap = 4096, n = 0;
    double *rec = xcalloc(cap, sizeof(double));
    static char line[LINE_MAX_LEN];
    static double vals[LINE_MAX_LEN / 2];          /* cota: un número cada 2 caracteres */
    int bad_column = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') { ri.header_lines++; continue; }
        int m = parse_numeric_line(line, vals);
        if (m == 0) continue;                       /* línea vacía */
        if (m < 0) { ri.header_lines++; continue; } /* cabecera */
        if (ri.columns == 0) ri.columns = m;
        if (column > m) { bad_column = 1; break; }
        int first = column > 0 ? column - 1 : 0, last = column > 0 ? column : m;
        for (int t = first; t < last; t++) {
            if (n == cap) {
                cap *= 2;
                double *bigger = realloc(rec, cap * sizeof(double));
                if (!bigger) { fprintf(stderr, "Error: sin memoria leyendo %s\n", path); exit(EXIT_FAILURE); }
                rec = bigger;
            }
            rec[n++] = vals[t] * units_factor;
        }
    }
    fclose(f);
    if (bad_column || n == 0) {
        free(rec);
        if (info) *info = ri;
        return -2;
    }

    ri.samples = (int)n;
    for (size_t i = 0; i < n; i++) ri.pga = fmax(ri.pga, fabs(rec[i]));
    for (int s = 0; s < nsteps; s++) {
        double x = s * sim_dt / record_dt;          /* posición en el registro */
        size_t i = (size_t)x;
        if (i + 1 < n)      out[s] = rec[i] + (x - (double)i) * (rec[i + 1] - rec[i]);
        else if (i < n)     out[s] = rec[i];
        else                out[s] = 0.0;
    }
    free(rec);
    if (info) *info = ri;
    return (int)n;
}
