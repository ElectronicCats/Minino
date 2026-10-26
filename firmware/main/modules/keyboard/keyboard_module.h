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
 * Creates the dedicated input task and registers the five buttons. Button
 * events are queued to that task, so app callbacks never run on the esp_timer
 * task (priority 22, 3584-byte stack) where iot_button polls its pins.
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

/**
 * @brief Number of button events dropped because the input queue was full
 *
 * Non-zero means an app callback blocked long enough for input to back up.
 * Useful from the console to tell "the device is busy" from "the buttons do not
 * work".
 */
uint32_t keyboard_module_dropped_events(void);

/**
 * @brief Inject a synthetic button event into the input queue
 *
 * Same path a real button takes, from the iot_button callback through the
 * queue to the input task and the installed input_callback. Exists so the input
 * pipeline can be driven from the console (`press <button> [event]`) without
 * hardware - the screensaver wake-up and the menu render are only reachable
 * through a button press, and this is the part most likely to regress.
 *
 * @param button  BUTTON_BOOT / BUTTON_LEFT / BUTTON_RIGHT / BUTTON_UP /
 * BUTTON_DOWN
 * @param event   a button_event_t value; defaults to BUTTON_PRESS_DOWN if 0
 * @return true if the event was queued
 */
bool keyboard_module_inject_event(uint8_t button, uint8_t event);