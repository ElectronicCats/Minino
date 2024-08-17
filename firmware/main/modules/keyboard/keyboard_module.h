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

/**
 * @brief Enum of the available keyboard buttons
 *
 */
typedef enum {
  BUTTON_BOOT = 0,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_UP,
  BUTTON_DOWN,
} keyboard_buttons_layout_t;

/**
 * @brief Struct to hold the button state
 *
 */
typedef struct {
  uint8_t button_pressed;
  uint8_t button_event;
} button_event_state_t;

typedef void (*input_callback_t)(uint8_t, uint8_t);

/**
 * @brief Initialize the keyboard button
 *
 * @param uint32_t button_pin Button pin
 * @param uint8_t mask Mask
 * @param void handler function pointer for the button event callback
 *
 * @return void
 */
void keyboard_module_begin();

void keyboard_module_reset_idle_timer();

void keyboard_module_set_lock(bool lock);

void keyboard_module_set_input_callback(input_callback_t input_cb);