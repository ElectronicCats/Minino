#pragma once
#include <stdio.h>

static void periodic_animations_timer_callback();
void animations_timer_run(void* animation_cb, uint32_t period_ms);
void animations_timer_stop();