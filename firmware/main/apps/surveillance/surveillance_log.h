// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "gps_module.h"
#include "surv_types.h"

#define SURV_DIR_NAME    "surveil"
#define SURV_CSV_LINE    200
#define SURV_CSV_LINES   20
#define SURV_CSV_BUF_SZ  (SURV_CSV_LINE * SURV_CSV_LINES)

esp_err_t surveillance_log_begin(void);
void surveillance_log_detection(const surv_event_t* ev,
                                uint8_t score,
                                const gps_t* gps);
void surveillance_log_gpx_waypoint(const surv_event_t* ev,
                                   const gps_t* gps);
void surveillance_log_flush(void);
