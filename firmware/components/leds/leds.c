#include "leds.h"

#include <string.h>

#include "driver/ledc.h"
#include "ledc_controller.h"

#define LEFT_LED_IO       GPIO_NUM_3
#define RIGHT_LED_IO      GPIO_NUM_11
#define LEFT_LED_CHANNEL  LEDC_CHANNEL_0
#define RIGHT_LED_CHANNEL LEDC_CHANNEL_1
#define LEDC_TIMER        LEDC_TIMER_0

static led_t *left_led, *right_led;

void leds_begin() {
  left_led = (led_t*) malloc(sizeof(led_t));
  right_led = (led_t*) malloc(sizeof(led_t));
  *left_led = led_controller_led_new(LEFT_LED_IO, LEFT_LED_CHANNEL);
  *right_led = led_controller_led_new(RIGHT_LED_IO, RIGHT_LED_CHANNEL);
  led_controller_led_init(left_led);
  led_controller_led_init(right_led);
}

void leds_deinit() {
  if (!left_led || !right_led) {
    return;
  }
  led_controller_led_deinit(left_led);
  led_controller_led_deinit(right_led);
  free(left_led);
  free(right_led);
  left_led = NULL;
  right_led = NULL;
}

void leds_on() {
  led_controller_led_on(left_led);
  led_controller_led_on(right_led);
}

void leds_off() {
  led_controller_led_off(left_led);
  led_controller_led_off(right_led);
}

void leds_set_brightness(uint8_t led, uint8_t brightness) {
  led_controller_set_duty(led == LED_LEFT ? left_led : right_led, brightness);
}

void led_left_on() {
  led_controller_led_on(left_led);
}

void led_left_off() {
  led_controller_led_off(left_led);
}

void led_right_on() {
  led_controller_led_on(right_led);
}

void led_right_off() {
  led_controller_led_off(right_led);
}

void led_start_blink(uint8_t led,
                     uint8_t duty,
                     uint8_t pulse_count,
                     uint32_t time_on,
                     uint32_t time_off,
                     uint32_t time_out) {
  led_controller_start_blink_effect(led == LED_LEFT ? left_led : right_led,
                                    duty, pulse_count, time_on, time_off,
                                    time_out);
}
void led_start_breath(uint8_t led, uint16_t period_ms) {
  led_controller_start_breath_effect(led == LED_LEFT ? left_led : right_led,
                                     period_ms);
}
void led_stop(uint8_t led) {
  led_controller_stop_any_effect(led == LED_LEFT ? left_led : right_led);
}
