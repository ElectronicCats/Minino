#pragma once

#include <stdio.h>

#include "gps_module.h"

#define AGNSS_OPTIONS_PREF_KEY    "gpsagnss"
#define POWER_OPTIONS_PREF_KEY    "gpspower"
#define ADVANCED_OPTIONS_PREF_KEY "gpsadvanced"

void gps_screens_update_handler(gps_t* gps);
void gps_screens_show_waiting_signal();
void gps_screens_show_help();
void gps_screens_stop_route_recording();
void gps_screen_running_test(void);
void gps_screens_show_config(void);