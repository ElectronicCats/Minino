#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>
#include <stdint.h>

#include "ledc_controller.h"

typedef enum { LED_LEFT, LED_RIGHT } leds_enum;

void leds_begin();
void leds_deinit();
void leds_on();
void leds_off();
void leds_set_brightness(uint8_t led, uint8_t brightness);
void led_left_on();
void led_left_off();
void led_right_on();
void led_right_off();
void led_start_blink(uint8_t led,
                     uint8_t duty,
                     uint8_t pulse_count,
                     uint32_t time_on,
                     uint32_t time_off,
                     uint32_t time_out);
void led_start_breath(uint8_t led, uint16_t period_ms);
void led_stop(uint8_t led);

#endif  // LEDS_H
