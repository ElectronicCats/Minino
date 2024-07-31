/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_callback)(bool state);
// Register WiFi functions
void register_wifi(void);
int connect_wifi(const char* ssid, const char* pass, app_callback cb);
bool is_wifi_connected(void);

#define WIFI_AP_SSID            "MININO_AP"  // MAX 32
#define WIFI_AP_PASSWORD        "Cats1234"   // MAX 64
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_SSID_HIDDEN     0
#define WIFI_AP_MAX_CONNECTIONS 5
#define WIFI_AP_BEACON_INTERVAL 100
#define WIFI_AP_IP              "192.168.0.1"
#define WIFI_AP_GATEWAY         "192.168.0.1"
#define WIFI_AP_NETMASK         "255.255.255.0"
#define WIFI_AP_BANDWIDTH       WIFI_BW_HT20
#define WIFI_STA_POWER_SAVE     WIFI_PS_NONE
#define WIFI_AUTH_MODE          WIFI_AUTH_WPA2_PSK

#define WIFI_AP_CONFIG()                          \
  {                                               \
    .ap = {                                       \
      .ssid = WIFI_AP_SSID,                       \
      .ssid_len = strlen(WIFI_AP_SSID),           \
      .password = WIFI_AP_PASSWORD,               \
      .max_connection = WIFI_AP_MAX_CONNECTIONS,  \
      .channel = WIFI_AP_CHANNEL,                 \
      .ssid_hidden = WIFI_AP_SSID_HIDDEN,         \
      .authmode = WIFI_AUTH_MODE,                 \
      .beacon_interval = WIFI_AP_BEACON_INTERVAL, \
    }                                             \
  }

#ifdef __cplusplus
}
#endif
