#pragma once

#include <stdio.h>

#include "gps_module.h"

void gps_screens_update_handler(gps_t* gps);
void gps_screens_show_waiting_signal();
void gps_screens_show_help();
void gps_screens_stop_route_recording();
void gps_screen_running_test(void);
void gps_screens_show_config(void);