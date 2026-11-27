#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BUTTON_PRESS_DOWN 0
#define BUTTON_LEFT       0
#define BUTTON_RIGHT      1
#define BUTTON_UP         2
#define BUTTON_DOWN       3
static inline void menus_module_set_app_state(bool st,
                                              void (*cb)(uint8_t, uint8_t)) {
  (void) st;
  (void) cb;
}
static inline void menus_module_set_default_input(void) {}
static inline void menus_module_exit_app(void) {}
