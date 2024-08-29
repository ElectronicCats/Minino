#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

#include "buzzer.h"

#define LEDC_TIMER                  LEDC_TIMER_1
#define LEDC_MODE                   LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL                LEDC_CHANNEL_2
#define LEDC_DUTY_RES               LEDC_TIMER_13_BIT  // Set duty resolution to 13 bits
#define BUZZER_DEFAULT_DUTTY        (4096)  // Set duty to 50%. (2 ** 13) * 50% = 4096
#define BUZZER_DEFAULT_FREQUENCY_HZ (4000)  // Set frequency at 4 kHz

static const char* TAG = "buzzer";

typedef struct {
  uint8_t pin;
  uint32_t freq;
  uint32_t duty;
  bool enabled;
} buzzer_t;

static buzzer_t buzzer;

void buzzer_enable() {
  buzzer.enabled = true;
}
void buzzer_disable() {
  buzzer.enabled = false;
}

void buzzer_begin(uint8_t pin) {
  buzzer.pin = pin;
  buzzer.freq = BUZZER_DEFAULT_FREQUENCY_HZ;
  buzzer.duty = BUZZER_DEFAULT_DUTTY;
}

void buzzer_configure() {
  // Prepare and then apply the LEDC PWM timer configuration
  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_MODE,
                                    .duty_resolution = LEDC_DUTY_RES,
                                    .timer_num = LEDC_TIMER,
                                    .freq_hz = buzzer.freq,
                                    .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // Prepare and then apply the LEDC PWM channel configuration
  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .timer_sel = LEDC_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = buzzer.pin,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void buzzer_set_freq(uint32_t freq) {
  buzzer.freq = freq;
}

void buzzer_set_duty(uint32_t duty) {
  buzzer.duty = duty;
}

void buzzer_play() {
  if (!buzzer.enabled) {
    return;
  }
  buzzer_configure();
  // Set the duty cycle
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, buzzer.duty));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void buzzer_play_for_task(void* duration) {
  uint32_t dur = *(uint32_t*) duration;
  buzzer_play();
  vTaskDelay(*(uint32_t*) duration / portTICK_PERIOD_MS);
  buzzer_stop();
  vTaskDelete(NULL);
}

void buzzer_play_for(uint32_t duration) {
  if (!buzzer.enabled) {
    return;
  }
  uint32_t* duration_ptr = malloc(sizeof(uint32_t));
  *duration_ptr = duration;
  xTaskCreate(buzzer_play_for_task, "buzzer_play_for_task", 2048, duration_ptr,
              5, NULL);
}

void buzzer_stop() {
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

  // Configure buzzer_pin as input
  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << buzzer.pin),
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};

  ESP_ERROR_CHECK(gpio_config(&io_conf));
}
