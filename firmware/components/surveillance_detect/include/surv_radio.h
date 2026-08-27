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

// Ejecuta UNA ventana de la fase pedida y, al terminar, llama
// on_phase_done() (que flipa la fase persistida y reinicia el chip). Solo
// para SURV_PROFILE_SURVEIL: en el ESP32-C6 cada stack de radio solo
// funciona bien como primer usuario de cada boot, y el re-init BLE tras un
// ciclo WiFi falla (0x1), asi que "Scan All" alterna con reinicios.
esp_err_t surv_radio_start_once(surv_profile_t p, bool active_scan,
                                surv_radio_phase_t phase,
                                void (*on_phase_done)(void));

bool surv_queue_push(const surv_event_t* ev, uint8_t points);
void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type);
