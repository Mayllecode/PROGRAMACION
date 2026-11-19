/*
 * consola.c — Lo mínimo para una interfaz en la terminal: colores ANSI,
 * UTF-8, leer teclas sin esperar Enter y el tamaño de la ventana.
 */
#include "consola.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static DWORD modo_out, modo_in;
static UINT cp_out;

void con_iniciar(void) {
    HANDLE ho = GetStdHandle(STD_OUTPUT_HANDLE), hi = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(ho, &modo_out);
    GetConsoleMode(hi, &modo_in);
    cp_out = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleMode(ho, modo_out | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    /* sin "edición rápida": un clic en la ventana no congela el programa */
    SetConsoleMode(hi, (modo_in & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
}

void con_restaurar(void) {
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), modo_out);
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), modo_in);
    SetConsoleOutputCP(cp_out);
}

int con_tecla(void) {
    if (!_kbhit()) return -1;
    int c = _getch();
    if (c == 0 || c == 224) return 1000 + _getch();   /* flechas y teclas especiales */
    return c;
}

void con_tamano(int *ancho, int *alto) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        *ancho = info.srWindow.Right - info.srWindow.Left + 1;
        *alto = info.srWindow.Bottom - info.srWindow.Top + 1;
    } else {
        *ancho = 120; *alto = 40;
    }
}

#else   /* Linux / WSL / macOS */
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig;

void con_iniciar(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &orig);
    t = orig;
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void con_restaurar(void) { tcsetattr(STDIN_FILENO, TCSANOW, &orig); }

static int leer(void) {
    fd_set fs;
    struct timeval tv = {0, 0};
    FD_ZERO(&fs);
    FD_SET(STDIN_FILENO, &fs);
    if (select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
}

int con_tecla(void) {
    int c = leer();
    if (c != 27) return c;
    int c2 = leer();
    if (c2 != '[') return 27;
    switch (leer()) {   /* mismos códigos que en Windows */
        case 'A': return 1072;
        case 'B': return 1080;
        case 'C': return 1077;
        case 'D': return 1075;
    }
    return -1;
}

void con_tamano(int *ancho, int *alto) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        *ancho = w.ws_col; *alto = w.ws_row;
    } else {
        *ancho = 120; *alto = 40;
    }
}
#endif
