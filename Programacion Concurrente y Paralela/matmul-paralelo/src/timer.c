#include "timer.h"

#ifdef _WIN32
/* Windows (MinGW/WinLibs, MSVC): el contador de alto rendimiento es el
 * reloj monotónico recomendado por el sistema. */
#include <windows.h>

double now_sec(void) {
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>

double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif
