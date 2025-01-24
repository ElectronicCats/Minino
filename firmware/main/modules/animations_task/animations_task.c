#include "animations_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

void (*animations_task_cb)(void*) = NULL;

static bool running = false;
static uint32_t delay_ms = 100;

static void animations_task(void* pvParameters) {
  running = true;
  while (running) {
    if (animations_task_cb != NULL) {
      animations_task_cb(pvParameters);
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
  vTaskDelete(NULL);
}

void animations_task_run(void* animation_cb,
                         uint32_t period_ms,
                         void* pvParameters) {
  if (preferences_get_bool("stealth_mode", false)) {
    return;
  }
  animations_task_cb = animation_cb;
  delay_ms = period_ms;
  xTaskCreate(animations_task, "animations_task", 2048, pvParameters, 5, NULL);
}

void animations_task_stop() {
  animations_task_cb = NULL;
  running = false;
}