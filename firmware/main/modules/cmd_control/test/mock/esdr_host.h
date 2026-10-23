/* Host-test stub for the SDR host mode.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The command control calls into the host mode, which talks to the USB
 * port: the test only needs the declaration and a stub that does nothing.
 */
#pragma once

typedef int esp_err_t;

esp_err_t esdr_host_run(void);
