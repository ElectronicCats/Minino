#include "animations_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

static const char* TAG = "animations_task";

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
  // Safety net: the task did not exit in time. Deleting it here is still unsafe
  // (it may sit inside an OLED primitive holding oled_mutex), and that would
  // wedge the display for the rest of the session - including menus_module.c's
  // input path, so the user could not navigate out. Wait longer instead and
  // only then leave the task alone; the reboot it will ask for on the next
  // reset clears the mutex.
  if (animations_task_handle != NULL) {
    ESP_LOGW(TAG,
             "animation task still busy after 500 ms, waiting longer "
             "instead of force-deleting it");
    uint16_t extra = 150;  // up to 1.5 s more
    while (animations_task_handle != NULL && extra-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}