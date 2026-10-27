// SPDX-License-Identifier: GPL-3.0-or-later
// Radio control (BLE/WiFi) for the surveillance detector.
// Stub in this task; real implementation: Task 10.
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

// Runs ONE window of the requested phase and, when finished, calls
// on_phase_done() (which flips the persisted phase and reboots the chip). Only
// for SURV_PROFILE_SURVEIL: on the ESP32-C6 each radio stack only works
// reliably as the first user of a fresh boot, and BLE re-init after a WiFi
// cycle fails (0x1), so "Scan All" alternates across reboots.
esp_err_t surv_radio_start_once(surv_profile_t p,
                                bool active_scan,
                                surv_radio_phase_t phase,
                                void (*on_phase_done)(void));

bool surv_queue_push(const surv_event_t* ev, uint8_t points);
void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type);
