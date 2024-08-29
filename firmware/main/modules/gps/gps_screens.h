#pragma once

#include <stdio.h>

#include "gps_module.h"

void gps_screens_update_handler(gps_t* gps);
void gps_screens_show_waiting_signal();
void gps_screens_show_help();