#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define LEDC_TIMER                  LEDC_TIMER_1
#define LEDC_MODE                   LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL                LEDC_CHANNEL_2
#define LEDC_DUTY_RES               LEDC_TIMER_13_BIT  // 13 bits resolution
#define BUZZER_DEFAULT_DUTTY        (4096)             // 50% duty
#define BUZZER_DEFAULT_FREQUENCY_HZ (4000)             // 4 kHz

typedef struct {
  uint8_t pin;
  uint32_t freq;
  uint32_t duty;
  bool enabled;
  esp_timer_handle_t timer_handle;
} buzzer_t;

static buzzer_t buzzer = {
    .pin = 2,
    .freq = BUZZER_DEFAULT_FREQUENCY_HZ,
    .duty = BUZZER_DEFAULT_DUTTY,
    .enabled = true,
    .timer_handle = NULL,
};

static void buzzer_timer_callback(void* arg) {
  buzzer_stop();
}

void buzzer_enable() {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif
  buzzer.enabled = true;
}

void buzzer_disable() {
  buzzer.enabled = false;
  buzzer_stop();
}

void buzzer_begin(uint8_t pin) {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif

  buzzer.pin = pin;
  buzzer.freq = BUZZER_DEFAULT_FREQUENCY_HZ;
  buzzer.duty = BUZZER_DEFAULT_DUTTY;

  if (buzzer.timer_handle == NULL) {
    const esp_timer_create_args_t timer_args = {
        .callback = buzzer_timer_callback,
        .arg = NULL,
        .name = "buzzer_timer",
    };
    esp_timer_create(&timer_args, &buzzer.timer_handle);
  }
}

void buzzer_configure() {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif

  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_MODE,
      .duty_resolution = LEDC_DUTY_RES,
      .timer_num = LEDC_TIMER,
      .freq_hz = buzzer.freq,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .speed_mode = LEDC_MODE,
      .channel = LEDC_CHANNEL,
      .timer_sel = LEDC_TIMER,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = buzzer.pin,
      .duty = 0,
      .hpoint = 0,
  };
  ledc_channel_config(&ledc_channel);
}

void buzzer_set_freq(uint32_t freq) {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif
  buzzer.freq = freq;
}

void buzzer_set_duty(uint32_t duty) {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif
  buzzer.duty = duty;
}

void buzzer_play() {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif

  if (!buzzer.enabled) {
    return;
  }
  buzzer_configure();
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, buzzer.duty);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void buzzer_play_for(uint32_t duration) {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif

  if (!buzzer.enabled) {
    return;
  }

  if (buzzer.timer_handle != NULL) {
    esp_timer_stop(buzzer.timer_handle);
  }

  buzzer_play();

  if (buzzer.timer_handle != NULL) {
    esp_timer_start_once(buzzer.timer_handle, (uint64_t) duration * 1000);
  }
}

void buzzer_stop() {
#ifndef CONFIG_BUZZER_COMPONENT_ENABLED
  return;
#endif

  if (buzzer.timer_handle != NULL) {
    esp_timer_stop(buzzer.timer_handle);
  }

  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}
