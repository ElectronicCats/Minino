// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

void trackers_screens_show_status(uint8_t tracker_count,
                                  const char* last_name,
                                  const char* last_vendor,
                                  int8_t rssi,
                                  float distance,
                                  bool gps_valid,
                                  double gps_lat,
                                  double gps_lon,
                                  bool scanning);

void trackers_screens_show_help(uint8_t page);
