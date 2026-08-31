// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

void surveillance_screens_show_status(uint8_t score,
                                      const char* profile,
                                      const char* last_label,
                                      uint8_t last_tier,
                                      int8_t rssi,
                                      uint8_t channel,
                                      uint32_t overflows);

void surveillance_screens_show_help(uint8_t page);

void surveillance_screens_show_radio_busy(void);
