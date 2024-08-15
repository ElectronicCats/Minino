#pragma once

#include "menus.h"

#include "keyboard_module.h"

typedef struct {
  bool in_app;
  input_callback_t input_callback;
} app_state2_t;

void menus_module_begin();
void menus_module_enable_input();
void menus_module_disable_input();
void menus_module_set_app_state(bool in_app, input_callback_t input_cb);