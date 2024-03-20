#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>
#include <stdint.h>

void leds_init();
void leds_on();
void leds_off();
bool leds_set_brightness(uint8_t brightness);
void led_left_on();
void led_left_off();
void led_right_on();
void led_right_off();

#endif  // LEDS_H