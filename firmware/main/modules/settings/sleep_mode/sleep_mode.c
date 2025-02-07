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
#include "preferences.h"

#define AFK_TIME_MEM          "afk_time"
#define SLEEP_MODE_ENABLE_MEM "sleep_enable"

static int AFK_TIMEOUT_S = 300;
static bool sleep_mode_enabled = false;
static const char* TAG = "sleep_mode";
static esp_timer_handle_t afk_timer;

void sleep_mode_reset_timer();

static void sleep_mode_sleep() {
  gpio_wakeup_enable(LEFT_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(RIGHT_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(UP_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(DOWN_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
  esp_err_t result = esp_sleep_enable_gpio_wakeup();

  if (result == ESP_OK) {
    oled_screen_get_last_buffer();
    oled_screen_clear();
    esp_light_sleep_start();

    vTaskDelay(pdMS_TO_TICKS(300));
    sleep_mode_reset_timer();
    gpio_wakeup_disable(LEFT_BUTTON_PIN);
    gpio_wakeup_disable(RIGHT_BUTTON_PIN);
    gpio_wakeup_disable(UP_BUTTON_PIN);
    gpio_wakeup_disable(DOWN_BUTTON_PIN);
    oled_screen_set_last_buffer();
  } else {
    printf("Error al habilitar el despertar por GPIO\n");
  }
}

static void timer_callback() {
  if (menus_module_get_app_state() || !sleep_mode_enabled) {
    return;
  }
  sleep_mode_sleep();
}

void sleep_mode_reset_timer() {
  esp_timer_stop(afk_timer);
  esp_timer_start_once(afk_timer, AFK_TIMEOUT_S * 1000 * 1000);
}

void sleep_mode_set_afk_timeout(int16_t timeout_seconds) {
  AFK_TIMEOUT_S = timeout_seconds < 10 ? 10 : timeout_seconds;
  sleep_mode_reset_timer();
  preferences_put_short(AFK_TIME_MEM, AFK_TIMEOUT_S);
}

void sleep_mode_set_enabled(bool enabled) {
  sleep_mode_enabled = enabled;
  preferences_put_bool(SLEEP_MODE_ENABLE_MEM, sleep_mode_enabled);
  if (!sleep_mode_enabled) {
    esp_timer_stop(afk_timer);
  }
};

void sleep_mode_begin() {
  esp_timer_create_args_t timer_args = {
      .callback = timer_callback, .arg = NULL, .name = "afk_timer"};
  esp_err_t err = esp_timer_create(&timer_args, &afk_timer);
  AFK_TIMEOUT_S = preferences_get_short(AFK_TIME_MEM, 300);
  sleep_mode_enabled = preferences_get_bool(SLEEP_MODE_ENABLE_MEM, true);
  sleep_mode_reset_timer();
}