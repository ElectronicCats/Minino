#pragma once

#include <stdbool.h>
#include <stdio.h>

void screen_saver_run();
void screen_saver_stop();
void screen_saver_begin();
bool screen_saver_get_idle_state();
void screen_saver_set_idle_timeout(uint8_t timeout_seconds);