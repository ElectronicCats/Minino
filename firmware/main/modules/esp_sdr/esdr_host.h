/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The "host mode": Minino turns the USB serial port into the receiver the
 * ESP-WebSDR browser viewer drives.
 *
 * The viewer speaks the burst protocol of the ESP-SDR firmware (see
 * esdr_burst_proto.h). While a session is running the port belongs to the
 * viewer: the console stops reading it, the logs are silenced so that
 * nothing interleaves with the binary captures, and the radio is held with
 * a lease so the rest of Minino does not fight for it.
 */

#ifndef ESDR_HOST_H_
#define ESDR_HOST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esdr_transport.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serve the burst protocol on the USB serial port until the client goes.
 *
 * Called from the console, so the port has exactly one reader: the console
 * task is between two reads when this runs.
 *
 * The session ends when the client sends RELEASE, when it sends the Minino
 * specific MININO escape, or when nothing arrives for a minute.
 *
 * @return ESP_OK when the session ended normally.
 */
esp_err_t esdr_host_run(void);

/**
 * @brief Request the running esdr_host session to stop gracefully.
 */
void esdr_host_request_stop(void);

/**
 * @brief Check if esdr_host session is currently active.
 */
bool esdr_host_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* ESDR_HOST_H_ */
