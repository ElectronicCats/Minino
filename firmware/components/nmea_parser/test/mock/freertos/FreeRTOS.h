// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms)  ((TickType_t) (ms))
#define pdTRUE             1
#define pdFALSE            0
#define portTICK_PERIOD_MS 1

static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) {
  int* dummy = (int*) malloc(sizeof(int));
  if (dummy)
    *dummy = 0;
  return (SemaphoreHandle_t) dummy;
}

static inline int xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks) {
  (void) sem;
  (void) ticks;
  return pdTRUE;
}

static inline int xSemaphoreGive(SemaphoreHandle_t sem) {
  (void) sem;
  return pdTRUE;
}

static inline void vSemaphoreDelete(SemaphoreHandle_t sem) {
  if (sem)
    free(sem);
}
