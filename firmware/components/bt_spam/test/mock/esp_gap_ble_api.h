/* Minimal stand-in for esp_gap_ble_api.h, used only by the host tests of
 * bt_spam_gap_sync.c. It declares exactly the enumerators the sync module and
 * its tests name, with the same values ESP-IDF uses, so a rename in the real
 * header surfaces here as a compile error instead of a silent behaviour change.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <stdint.h>

typedef enum {
  ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT = 0x0C,
  ESP_GAP_BLE_ADV_START_COMPLETE_EVT = 0x0D,
  ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT = 0x0E,
  ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT = 0x0F,
  ESP_GAP_BLE_ADV_TERMINATED_EVT = 0x12,
  ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT = 0x13,
  ESP_GAP_BLE_AUTH_CMPL_EVT = 0x08,
  ESP_GAP_BLE_SEC_REQ_EVT = 0x06,
} esp_gap_ble_cb_event_t;

typedef enum {
  ESP_BT_STATUS_SUCCESS = 0x0,
  ESP_BT_STATUS_FAIL = 0x1,
  ESP_BT_STATUS_PENDING = 0x2,
  ESP_BT_STATUS_BUSY = 0x3,
  ESP_BT_STATUS_INVALID_STATE = 0x4,
} esp_bt_status_t;