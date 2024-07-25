#include "animations_timer.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_timer_handle_t animations_timer_handle = NULL;
static void (*animations_timer_cb)(void) = NULL;
static void periodic_animations_timer_callback();

static void periodic_animations_timer_callback() {
  if (animations_timer_cb != NULL) {
    animations_timer_cb();
  }
}

void animations_timer_run(void* animation_cb, uint32_t period_ms) {
  const esp_timer_create_args_t animations_timer_args = {
      .callback = &periodic_animations_timer_callback,
      .arg = NULL,
      .name = "animations_timer"};

  esp_timer_create(&animations_timer_args, &animations_timer_handle);
  animations_timer_cb = animation_cb;
  esp_timer_start_periodic(animations_timer_handle, period_ms * 1000);
}

void animations_timer_stop() {
  if (esp_timer_is_active(animations_timer_handle)) {
    esp_timer_stop(animations_timer_handle);
    esp_timer_delete(animations_timer_handle);
  }
  animations_timer_cb = NULL;
  vTaskDelay(pdMS_TO_TICKS(100));
}