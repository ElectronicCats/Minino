// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "freertos/FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks) {
  (void) ticks;
}

static inline void vTaskDelete(TaskHandle_t task) {
  (void) task;
}
