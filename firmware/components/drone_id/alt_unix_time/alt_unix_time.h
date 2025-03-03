#ifndef ALT_TIME_H
#define ALT_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Declaración de la función que calcula los segundos desde 1/1/1970 (Unix
// timestamp alternativo)
uint64_t alt_unix_secs(int year,
                       int month,
                       int mday,
                       int hour,
                       int minute,
                       int second);

// Declaración de la función que calcula el día juliano para una fecha dada
double julian_day(int year, int month, float mday, int gregorian);

#ifdef __cplusplus
}
#endif

#endif  // ALT_TIME_H
