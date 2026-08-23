// SPDX-License-Identifier: GPL-3.0-or-later
// Control del radio (BLE/WiFi) para el detector de vigilancia.
// Stub en esta tarea; implementacion real: Task 10.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum { SURV_PROFILE_BLE = 0, SURV_PROFILE_WIFI } surv_profile_t;

esp_err_t surv_radio_start(surv_profile_t p, bool active_scan);
void surv_radio_stop(void);
uint8_t surv_radio_current_channel(void);
