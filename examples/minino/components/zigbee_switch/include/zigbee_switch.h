/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_on_off_switch Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define MAX_CHILDREN 10 /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE \
  false /* enable the install code policy for security */
#define HA_ONOFF_SWITCH_ENDPOINT 1 /* esp light switch device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK \
  (1l << 13) /* Zigbee primary channel mask use in the example */

#define ESP_ZB_ZC_CONFIG()                            \
  {                                                   \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,    \
    .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
    .nwk_cfg.zczr_cfg = {                             \
        .max_children = MAX_CHILDREN,                 \
    },                                                \
  }

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

typedef enum {
  CREATING_NETWORK = 0,
  CREATING_NETWORK_FAILED,
  WAITING_FOR_DEVICES,
  NO_DEVICES_FOUND,
  LIGHT_RELASED,
  CLOSING_NETWORK,
  LIGHT_PRESSED

} display_status_t;

typedef void (*display_status_cb_t)(uint8_t);

/**
 * @brief Zigbee switch initialization
 *
 * @return void
 */
void zigbee_switch_init();

/**
 * @brief Zigbee switch deinitialization
 *
 * @return void
 */
void zigbee_switch_deinit();

/**
 * @brief Zigbee switch toggle
 *
 * @return void
 */
void zigbee_switch_toggle();

void zigbee_switch_set_display_status_cb(display_status_cb_t cb);
void zigbee_switch_remove_display_status_cb(display_status_cb_t cb);
bool zigbee_switch_is_light_connected();