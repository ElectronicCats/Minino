#include "sleep_mode.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "menus_module.h"
#include "oled_screen.h"

#define WAKEUP_PIN GPIO_NUM_1

static int AFK_TIMEOUT_S = 10;
static const char* TAG = "sleep_mode";
static esp_timer_handle_t afk_timer;

void sleep_mode_reset_timer();

static void sleep_mode_sleep() {
  esp_sleep_enable_ext1_wakeup_io((1ULL << WAKEUP_PIN),
                                  ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

static void timer_callback() {
  if (menus_module_get_app_state()) {
    return;
  }
  oled_screen_clear();
  sleep_mode_sleep();
}

void sleep_mode_reset_timer() {
  esp_timer_stop(afk_timer);
  esp_timer_start_once(afk_timer, AFK_TIMEOUT_S * 1000 * 1000);
}

void sleep_mode_set_afk_timeout(uint8_t timeout_seconds) {
  AFK_TIMEOUT_S = timeout_seconds;
  sleep_mode_reset_timer();
}

void sleep_mode_begin() {
  esp_timer_create_args_t timer_args = {
      .callback = timer_callback, .arg = NULL, .name = "afk_timer"};
  esp_err_t err = esp_timer_create(&timer_args, &afk_timer);
  sleep_mode_reset_timer();
}
