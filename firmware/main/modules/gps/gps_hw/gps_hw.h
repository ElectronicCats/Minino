#ifndef _GPS_HW_H_
#define _GPS_HW_H_

#include <stdbool.h>
#include <stdio.h>

#define GPS_ENABLED_MEM "gps_enabled"

void gps_hw_init();
void gps_hw_on();
void gps_hw_off();
bool gps_hw_get_state();

#endif  // _GPS_HW_H_