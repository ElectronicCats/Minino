#include "leds.h"

#include <string.h>

#include "driver/ledc.h"
#include "ledc_controller.h"

#define LEFT_LED_IO       GPIO_NUM_3
#define RIGHT_LED_IO      GPIO_NUM_11
#define LEFT_LED_CHANNEL  LEDC_CHANNEL_0
#define RIGHT_LED_CHANNEL LEDC_CHANNEL_1
#define LEDC_TIMER        LEDC_TIMER_0

static led_t s_left_led;
static led_t s_right_led;
static bool s_leds_initialized = false;

void leds_begin() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (s_leds_initialized) {
    return;
  }
  s_left_led = led_controller_led_new(LEFT_LED_IO, LEFT_LED_CHANNEL);
  s_right_led = led_controller_led_new(RIGHT_LED_IO, RIGHT_LED_CHANNEL);
  led_controller_led_init(&s_left_led);
  led_controller_led_init(&s_right_led);
  s_leds_initialized = true;
}

void leds_deinit() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) {
    return;
  }
  led_controller_led_deinit(&s_left_led);
  led_controller_led_deinit(&s_right_led);
  s_leds_initialized = false;
}

void leds_on() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_on(&s_left_led);
  led_controller_led_on(&s_right_led);
}

void leds_off() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_off(&s_left_led);
  led_controller_led_off(&s_right_led);
}

void leds_set_brightness(uint8_t led, uint8_t brightness) {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_set_duty(led == LED_LEFT ? &s_left_led : &s_right_led, brightness);
}

void led_left_on() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_on(&s_left_led);
}

void led_left_off() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_off(&s_left_led);
}

void led_right_on() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_on(&s_right_led);
}

void led_right_off() {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_led_off(&s_right_led);
}

void led_start_blink(uint8_t led,
                     uint8_t duty,
                     uint8_t pulse_count,
                     uint32_t time_on,
                     uint32_t time_off,
                     uint32_t time_out) {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_start_blink_effect(led == LED_LEFT ? &s_left_led : &s_right_led,
                                    duty, pulse_count, time_on, time_off,
                                    time_out);
}
void led_start_breath(uint8_t led, uint16_t period_ms) {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_start_breath_effect(led == LED_LEFT ? &s_left_led : &s_right_led,
                                     period_ms);
}
void led_stop(uint8_t led) {
#ifndef CONFIG_LEDS_COMPONENT_ENABLED
  return;
#endif

  if (!s_leds_initialized) return;
  led_controller_stop_any_effect(led == LED_LEFT ? &s_left_led : &s_right_led);
}
