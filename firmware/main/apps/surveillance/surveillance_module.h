// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

void surveillance_module_begin(void);
void surveillance_module_begin_all(void);
void surveillance_module_begin_flock(void);
void surveillance_module_begin_trackers(void);
void surveillance_module_show_help(void);
void surveillance_module_stop(void);
