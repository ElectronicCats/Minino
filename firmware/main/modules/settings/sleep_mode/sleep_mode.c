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

#define WAKEUP_PIN            GPIO_NUM_1
#define AFK_TIME_MEM          "afk_time"
#define SLEEP_MODE_ENABLE_MEM "sleep_enable"

#define GPS_POWER_PIN GPIO_NUM_8

static int AFK_TIMEOUT_S = AFK_TIMEOUT_DEFAULT_S;
static bool sleep_mode_enabled = false;
static bool sleep_mode = SLEEP_LIGHT_MODE;
static const char* TAG = "sleep_mode";
static esp_timer_handle_t afk_timer;

void sleep_mode_reset_timer();

static void sleep_mode_deep_sleep() {
  oled_screen_clear();
  esp_sleep_enable_ext1_wakeup((1ULL << WAKEUP_PIN), ESP_EXT1_WAKEUP_ANY_LOW);
  rtc_gpio_pullup_en(GPIO_NUM_1);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  rtc_gpio_hold_en(GPIO_NUM_1);
  esp_deep_sleep_start();
}

static void sleep_mode_light_sleep() {
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
  if (sleep_mode == SLEEP_LIGHT_MODE) {
    sleep_mode_light_sleep();
  } else {
    sleep_mode_deep_sleep();
  }
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
  AFK_TIMEOUT_S = preferences_get_short(AFK_TIME_MEM, AFK_TIMEOUT_DEFAULT_S);
  sleep_mode_enabled = preferences_get_bool(SLEEP_MODE_ENABLE_MEM, true);
  sleep_mode_reset_timer();
}

void sleep_mode_set_mode(sleep_modes_e mode) {
  sleep_mode = mode;
}