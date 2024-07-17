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

typedef void (*app_callback)(void);
// Register WiFi functions
void register_wifi(void);
int connect_wifi(const char* ssid, const char* pass, app_callback cb);
bool is_wifi_connected(void);

#ifdef __cplusplus
}
#endif
