#ifndef ALT_TIME_H
#define ALT_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Declaration of the function that calculates seconds since 1/1/1970
// (alternative Unix timestamp)
uint64_t alt_unix_secs(int year,
                       int month,
                       int mday,
                       int hour,
                       int minute,
                       int second);

// Declaration of the function that calculates the Julian day for a given date
double julian_day(int year, int month, float mday, int gregorian);

#ifdef __cplusplus
}
#endif

#endif  // ALT_TIME_H
