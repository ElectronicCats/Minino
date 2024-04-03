#include "leds.h"
#include "driver/ledc.h"

#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEFT_LED_IO    (GPIO_NUM_3)   // Define the output GPIO
#define RIGHT_LED_IO   (GPIO_NUM_11)  // Define the output GPIO
#define LEDC_CHANNEL   LEDC_CHANNEL_1
#define LEDC_DUTY_RES  LEDC_TIMER_13_BIT  // Set duty resolution to 13 bits
#define LEDC_DUTY      (4096)  // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY (4000)  // Frequency in Hertz. Set frequency at 4 kHz

void leds_init() {
  // Prepare and then apply the LEDC PWM timer configuration
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_MODE,
      .duty_resolution = LEDC_DUTY_RES,
      .timer_num = LEDC_TIMER,
      .freq_hz = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
      .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // Prepare and then apply the LEDC PWM channel configuration
  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .timer_sel = LEDC_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = LEFT_LED_IO,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  ledc_channel.gpio_num = RIGHT_LED_IO;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void leds_on() {
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void leds_off() {
  ESP_ERROR_CHECK(ledc_timer_pause(LEDC_MODE, LEDC_TIMER));

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .timer_sel = LEDC_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = LEFT_LED_IO,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  ledc_channel.gpio_num = RIGHT_LED_IO;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  ESP_ERROR_CHECK(ledc_timer_resume(LEDC_MODE, LEDC_TIMER));
}

bool leds_set_brightness(uint8_t brightness) {
  if (brightness > 100) {
    return false;
  }

  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, brightness * 32));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

  return true;
}

void led_right_on() {
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void led_right_off() {
  ESP_ERROR_CHECK(ledc_timer_pause(LEDC_MODE, LEDC_TIMER));

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .timer_sel = LEDC_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = RIGHT_LED_IO,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  ESP_ERROR_CHECK(ledc_timer_resume(LEDC_MODE, LEDC_TIMER));
}

void led_left_on() {
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void led_left_off() {
  ESP_ERROR_CHECK(ledc_timer_pause(LEDC_MODE, LEDC_TIMER));

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .timer_sel = LEDC_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = LEFT_LED_IO,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  ESP_ERROR_CHECK(ledc_timer_resume(LEDC_MODE, LEDC_TIMER));
}
