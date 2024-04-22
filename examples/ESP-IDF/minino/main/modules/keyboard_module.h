#pragma once

#include "iot_button.h"

#define BOOT_BUTTON_PIN  CONFIG_BOOT_BUTTON
#define LEFT_BUTTON_PIN  CONFIG_GPIO_LEFT_BUTTON
#define RIGHT_BUTTON_PIN CONFIG_GPIO_RIGHT_BUTTON
#define UP_BUTTON_PIN    CONFIG_GPIO_UP_BUTTON
#define DOWN_BUTTON_PIN  CONFIG_GPIO_DOWN_BUTTON

#define BOOT_BUTTON_MASK  0b0000 << 4
#define LEFT_BUTTON_MASK  0b0001 << 4
#define RIGHT_BUTTON_MASK 0b0010 << 4
#define UP_BUTTON_MASK    0b0011 << 4
#define DOWN_BUTTON_MASK  0b0100 << 4

#define BUTTON_ACTIVE_LEVEL 0

static const char* button_events_table[] = {
    "BUTTON_PRESS_DOWN",      "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",  "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD", "BUTTON_LONG_PRESS_UP",
};

/**
 * @brief Events for the available keyboard buttons events
 *
 */
static const char* button_names[] = {
    "BOOT", "LEFT", "RIGHT", "UP", "DOWN",
};

/**
 * @brief Layout of the available keyboard buttons names
 *
 */
enum button_name {
  BUTTON_BOOT = 0,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_UP,
  BUTTON_DOWN,
};

void button_init(uint32_t button_num, uint8_t mask);
void keyboard_init();
