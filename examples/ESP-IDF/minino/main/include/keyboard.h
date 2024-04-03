#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "iot_button.h"

#define BOOT_BUTTON_PIN     GPIO_NUM_9
#define LEFT_BUTTON_PIN     GPIO_NUM_22
#define RIGHT_BUTTON_PIN    GPIO_NUM_1
#define UP_BUTTON_PIN       GPIO_NUM_15
#define DOWN_BUTTON_PIN     GPIO_NUM_23
#define BUTTON_ACTIVE_LEVEL 0

#define BOOT_BUTTON_MASK  0b0000 << 4
#define LEFT_BUTTON_MASK  0b0001 << 4
#define RIGHT_BUTTON_MASK 0b0010 << 4
#define UP_BUTTON_MASK    0b0011 << 4
#define DOWN_BUTTON_MASK  0b0100 << 4

void button_init(uint32_t button_num, uint8_t mask);
void keyboard_init();

#endif  // KEYBOARD_H