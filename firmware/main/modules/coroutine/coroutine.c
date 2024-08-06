#include "coroutine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void start_coroutine(void* routine, void* ctx) {
  xTaskCreate(routine, "coroutine_task", 4096, ctx, 10, NULL);
}