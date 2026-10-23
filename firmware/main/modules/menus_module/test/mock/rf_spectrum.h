/* Host-test stub for the RF Spectrum app header.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * menus_data.c only takes the address of these two callbacks; it never calls
 * them. Keeping the declarations here means the catalogue can be compiled and
 * inspected on the host without dragging in ESP-IDF. */
#pragma once

void rf_spectrum_app_begin(void);
void rf_spectrum_app_stop(void);
