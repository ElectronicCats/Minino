#pragma once
#include <stdio.h>

void animations_task_run(void* animation_cb, uint32_t period_ms, void* ctx);
void animations_task_stop();