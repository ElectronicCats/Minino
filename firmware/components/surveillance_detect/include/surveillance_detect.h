// SPDX-License-Identifier: GPL-3.0-or-later
// API publica del detector de vigilancia.
// Stub en esta tarea; implementacion real: Task 9.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "surv_radio.h"
#include "surv_types.h"

typedef void (*surv_event_cb_t)(const surv_event_t* evt);

esp_err_t surv_begin(surv_profile_t profile, bool active_scan);
void surv_stop(void);
void surv_register_cb(surv_event_cb_t cb);
uint32_t surv_queue_overflows(void);
