#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  SLEEP_LIGHT_MODE,
  SLEEP_DEEP_MODE,
} sleep_modes_e;

void sleep_mode_begin();
void sleep_mode_reset_timer();
void sleep_mode_set_enabled(bool enabled);
void sleep_mode_set_afk_timeout(int16_t timeout_seconds);
void sleep_mode_set_mode(sleep_modes_e mode);
