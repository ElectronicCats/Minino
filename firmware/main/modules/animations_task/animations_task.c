#include "animations_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

void (*animations_task_cb)(void*) = NULL;

static volatile bool running = false;
static uint32_t delay_ms = 100;
static TaskHandle_t animations_task_handle = NULL;

static void animations_task(void* pvParameters) {
  while (running) {
    if (animations_task_cb != NULL) {
      animations_task_cb(pvParameters);
    }
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
  // Clear handle before self-delete so animations_task_stop() knows we exited.
  animations_task_handle = NULL;
  vTaskDelete(NULL);
}

void animations_task_run(void* animation_cb,
                         uint32_t period_ms,
                         void* pvParameters) {
  if (preferences_get_bool("stealth_mode", false)) {
    return;
  }
  if (animations_task_handle != NULL) {
    animations_task_stop();
  }
  animations_task_cb = animation_cb;
  delay_ms = period_ms;
  running = true;
  xTaskCreate(animations_task, "animations_task", 4096, pvParameters, 5,
              &animations_task_handle);
}

void animations_task_stop() {
  animations_task_cb = NULL;
  running = false;
  if (animations_task_handle == NULL) {
    return;
  }
  // Wait for the task to finish its current OLED/I2C frame and exit naturally.
  // Force-killing with vTaskDelete() while the task holds oled_mutex causes a
  // permanent deadlock: the mutex is never released, blocking every subsequent
  // OLED call.
  uint8_t retries = 50;  // 50 × 10 ms = 500 ms max
  while (animations_task_handle != NULL && retries-- > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  // Safety net: if the task is still alive after 500 ms, force-delete.
  if (animations_task_handle != NULL) {
    vTaskDelete(animations_task_handle);
    animations_task_handle = NULL;
  }
}