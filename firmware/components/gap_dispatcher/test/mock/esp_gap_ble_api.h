/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdint.h>

typedef enum {
  ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT = 0x0C,
  ESP_GAP_BLE_ADV_START_COMPLETE_EVT = 0x0D,
  ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT = 0x0E,
} esp_gap_ble_cb_event_t;

typedef struct {
  uint32_t reserved;
} esp_ble_gap_cb_param_t;

typedef void (*esp_gap_ble_cb_t)(esp_gap_ble_cb_event_t event,
                                 esp_ble_gap_cb_param_t* param);
