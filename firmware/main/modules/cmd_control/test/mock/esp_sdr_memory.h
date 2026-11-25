/* Host-test stub for the SDR capture bank.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sdr_host asks whether this is an SDR boot before it serves: the test only
 * needs the declarations, the stubs are in test_cmd_parsing.c.
 */
#pragma once

#include <stdbool.h>

bool sdr_bank_is_reserved(void);
void sdr_bank_reboot_into_sdr(void);
