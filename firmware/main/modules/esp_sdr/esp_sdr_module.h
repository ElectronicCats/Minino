/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ESP-SDR Native In-Process Module for Minino OS.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Launch native ESP-SDR application in Minino OS.
 */
void esp_sdr_module_begin(void);

/**
 * @brief Stop native ESP-SDR application and release hardware resources.
 */
void esp_sdr_module_stop(void);

/**
 * @brief Check if native ESP-SDR application is currently running.
 */
bool esp_sdr_module_is_running(void);

#ifdef __cplusplus
}
#endif