#include "buzzer.h"
#include "driver/ledc.h"

#define LEDC_TIMER     LEDC_TIMER_1
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (GPIO_NUM_2)  // Define the output GPIO
#define LEDC_CHANNEL   LEDC_CHANNEL_0
#define LEDC_DUTY_RES  LEDC_TIMER_13_BIT  // Set duty resolution to 13 bits
#define LEDC_DUTY      (4096)  // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY (4000)  // Frequency in Hertz. Set frequency at 4 kHz

void buzzer_init() {
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
                                        .gpio_num = LEDC_OUTPUT_IO,
                                        .duty = 0,  // Set duty to 0%
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void buzzer_play() {
  buzzer_init();
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void buzzer_stop() {
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

  // Configure LEDC_OUTPUT_IO as input
  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << LEDC_OUTPUT_IO),
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};

  ESP_ERROR_CHECK(gpio_config(&io_conf));
}
