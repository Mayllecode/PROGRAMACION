#ifndef CONSOLA_H
#define CONSOLA_H

void con_iniciar(void);
void con_restaurar(void);
int con_tecla(void);   /* -1 si no hay tecla; flechas: 1072 arriba, 1080 abajo, 1075 izq., 1077 der. */
void con_tamano(int *ancho, int *alto);

#endif
