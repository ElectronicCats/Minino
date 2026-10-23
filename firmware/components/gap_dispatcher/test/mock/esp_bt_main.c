/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Test double for esp_bt_main.h. See mock/esp_bt_main.h for why.
 */
#include "esp_bt_main.h"

#include <stddef.h>

static esp_bluedroid_status_t s_status = ESP_BLUEDROID_STATUS_UNINITIALIZED;
static esp_gap_ble_cb_t s_installed = NULL;
static int s_register_calls = 0;

void gd_test_reset(void) {
  s_status = ESP_BLUEDROID_STATUS_UNINITIALIZED;
  s_installed = NULL;
  s_register_calls = 0;
}

void gd_test_set_bluedroid_status(esp_bluedroid_status_t status) {
  s_status = status;
}

esp_gap_ble_cb_t gd_test_installed_handler(void) {
  return s_installed;
}

void gd_test_forget_handler(void) {
  s_installed = NULL;
  s_register_calls = 0;
}

int gd_test_register_call_count(void) {
  return s_register_calls;
}

esp_bluedroid_status_t esp_bluedroid_get_status(void) {
  return s_status;
}

esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t cb) {
  /* Bluedroid rejects the call outright unless the stack is ENABLED. */
  if (s_status != ESP_BLUEDROID_STATUS_ENABLED) {
    return ESP_FAIL;
  }
  s_register_calls++;
  s_installed = cb;  /* Bluedroid keeps a single handler: last one wins. */
  return ESP_OK;
}