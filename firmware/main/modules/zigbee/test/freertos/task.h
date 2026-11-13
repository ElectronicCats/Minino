#pragma once
#include "freertos/FreeRTOS.h"
typedef void* TaskHandle_t;
static inline void vTaskDelay(uint32_t t) {
  (void) t;
}
static inline void vTaskDelete(TaskHandle_t t) {
  (void) t;
}
static inline int xTaskCreate(void (*fn)(void*),
                              const char* name,
                              uint32_t stack,
                              void* arg,
                              int prio,
                              TaskHandle_t* out) {
  (void) fn;
  (void) name;
  (void) stack;
  (void) arg;
  (void) prio;
  if (out)
    *out = (TaskHandle_t) 0x1;
  return 1;
}
