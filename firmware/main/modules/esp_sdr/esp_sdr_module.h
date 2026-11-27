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

/**
 * @brief Switch the native ESP-SDR application into Web Host mode and start
 *        serving the ESPARGOS burst protocol on the USB port.
 *
 * Returns when the session ends: the caller is the console, which must not
 * read the port meanwhile.
 *
 * @param first_line The line that opened the session (the viewer's SYNC),
 *        answered first; NULL when there is none.
 */
void esp_sdr_module_enter_web_host(const char* first_line);

#ifdef __cplusplus
}
#endif