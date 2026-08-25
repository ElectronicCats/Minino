// SPDX-License-Identifier: GPL-3.0-or-later
// Control del radio (BLE/WiFi) para el detector de vigilancia.
// Stub en esta tarea; implementacion real: Task 10.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "surveillance_detect.h"

esp_err_t surv_radio_start(surv_profile_t p, bool active_scan);
void surv_radio_stop(void);
uint8_t surv_radio_current_channel(void);

bool surv_queue_push(const surv_event_t* ev, uint8_t points);
void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type);
