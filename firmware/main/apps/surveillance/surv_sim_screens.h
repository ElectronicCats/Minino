// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

void surv_sim_screens_show(const char* mode_name,
                           const char* target_name,
                           uint32_t packet_count,
                           uint8_t channel,
                           bool is_ble);
