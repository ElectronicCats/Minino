#include "animations_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

void (*animations_task_cb)(void*) = NULL;

bool running = false;
uint32_t delay_ms = 100;

static void animations_task(void* ctx) {
  running = true;
  while (running) {
    if (animations_task_cb != NULL) {
      animations_task_cb(ctx);
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
  vTaskDelete(NULL);
}

void animations_task_run(void* animation_cb, uint32_t period_ms, void* ctx) {
  if (preferences_get_bool("stealth_mode", false)) {
    return;
  }
  animations_task_cb = animation_cb;
  delay_ms = period_ms;
  xTaskCreate(animations_task, "animations_task", 2048, ctx, 5, NULL);
}

void animations_task_stop() {
  animations_task_cb = NULL;
  running = false;
}