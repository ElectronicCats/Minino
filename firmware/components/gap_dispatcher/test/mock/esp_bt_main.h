/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Test double for Bluedroid's status/registration API. gap_dispatcher only ever
 * calls these two, and both behaviours matter: registering while Bluedroid is
 * not ENABLED has to be a no-op, and registering must overwrite whatever
 * handler was there before.
 */
#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

typedef enum {
  ESP_BLUEDROID_STATUS_UNINITIALIZED = 0x00,
  ESP_BLUEDROID_STATUS_INITIALIZED = 0x01,
  ESP_BLUEDROID_STATUS_ENABLED = 0x02,
} esp_bluedroid_status_t;

/* --- control surface for the tests --- */
void gd_test_reset(void);
void gd_test_set_bluedroid_status(esp_bluedroid_status_t status);
esp_gap_ble_cb_t gd_test_installed_handler(void);
int gd_test_register_call_count(void);
/* Stand in for a Bluedroid disable/enable cycle wiping its handler. */
void gd_test_forget_handler(void);

esp_bluedroid_status_t esp_bluedroid_get_status(void);
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t cb);
