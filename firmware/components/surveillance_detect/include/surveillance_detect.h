// SPDX-License-Identifier: GPL-3.0-or-later
// API publica del detector de vigilancia.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "surv_types.h"

typedef enum {
  SURV_PROFILE_FLOCK = 0,  // 100% WiFi promiscuo
  SURV_PROFILE_SURVEIL,    // ciclo BLE + WiFi
  SURV_PROFILE_TRACKERS,   // 100% BLE pasivo
} surv_profile_t;

typedef void (*surv_detect_cb_t)(const surv_event_t* ev, uint8_t score);

esp_err_t surv_begin(surv_profile_t profile, bool active_scan);
void surv_stop(void);
void surv_register_cb(surv_detect_cb_t cb);
uint32_t surv_queue_overflows(void);
