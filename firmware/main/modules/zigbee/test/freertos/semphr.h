#pragma once
#include "freertos/FreeRTOS.h"
typedef void* SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
  return (SemaphoreHandle_t) 0x1;
}
static inline int xSemaphoreTake(SemaphoreHandle_t m, uint32_t t) {
  (void) m;
  (void) t;
  return pdTRUE;
}
static inline void xSemaphoreGive(SemaphoreHandle_t m) {
  (void) m;
}
