/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "bt_spam_gap_sync.h"

void bt_spam_gap_sync_reset(bt_spam_gap_sync_t* sync) {
  if (sync == NULL) {
    return;
  }
  sync->completed = 0;
  sync->last_event = ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT;
  sync->last_status = ESP_BT_STATUS_SUCCESS;
}

bool bt_spam_gap_sync_complete(bt_spam_gap_sync_t* sync,
                               esp_gap_ble_cb_event_t event,
                               esp_bt_status_t status) {
  if (sync == NULL) {
    return false;
  }
  sync->completed++;
  sync->last_event = event;
  sync->last_status = status;
  return true;
}

uint32_t bt_spam_gap_sync_mark(const bt_spam_gap_sync_t* sync) {
  return (sync == NULL) ? 0 : sync->completed;
}

bool bt_spam_gap_sync_has_new(const bt_spam_gap_sync_t* sync, uint32_t mark) {
  if (sync == NULL) {
    return false;
  }
  return sync->completed != mark;
}

bool bt_spam_gap_sync_matches(const bt_spam_gap_sync_t* sync,
                              esp_gap_ble_cb_event_t wanted) {
  if (sync == NULL) {
    return false;
  }
  return sync->last_event == wanted;
}

bool bt_spam_gap_sync_succeeded(const bt_spam_gap_sync_t* sync) {
  if (sync == NULL) {
    return false;
  }
  return sync->last_status == ESP_BT_STATUS_SUCCESS;
}