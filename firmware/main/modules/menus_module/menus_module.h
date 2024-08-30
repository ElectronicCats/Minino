#pragma once

#include "menus.h"

#include "keyboard_module.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define SCREEN_WIDTH2  128
  #define SCREEN_HEIGHT2 64
#else  // CONFIG_RESOLUTION_128X32
  #define SCREEN_WIDTH2  128
  #define SCREEN_HEIGHT2 32
#endif

typedef struct {
  bool in_app;
  input_callback_t input_callback;
  input_callback_t input_last_callback;
} app_state2_t;

void menus_module_begin();
void menus_module_enable_input();
void menus_module_disable_input();
void menus_module_set_app_state(bool in_app, input_callback_t input_cb);
menu_idx_t menus_module_get_current_menu();
void menus_module_set_app_state_last();
bool menus_module_get_app_state();
void menus_module_restart();
void menus_module_exit_app();
void menus_module_screen_saver_run();
void menus_module_set_reset_screen(menu_idx_t menu);
