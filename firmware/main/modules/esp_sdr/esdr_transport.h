/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The transport the modules of the ESP-SDR protocol share: writing to the
 * port, asking whether the client wants to stop, and the clock of the
 * session.
 *
 * It is its own header, without ESP-IDF, so the modules that use it can be
 * tested on the host.
 */

#ifndef ESDR_TRANSPORT_H_
#define ESDR_TRANSPORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write bytes to the port.
 */
void esdr_host_write(const void* data, size_t len);

/**
 * @brief Whether the client asked to stop.
 *
 * The viewer cuts the spectrum stream with an empty line.
 *
 * @return true if there is something to read on the port.
 */
bool esdr_host_stop_requested(void);

/**
 * @brief Milliseconds since the session started.
 */
uint64_t esdr_host_elapsed_ms(void);

/**
 * @brief Give the CPU back for a moment.
 *
 * The task that watches the tasks has to run, or the watchdog fires.
 */
void esdr_host_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* ESDR_TRANSPORT_H_ */
