#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*input_callback_t)(uint8_t button_name, uint8_t button_event);

void menus_module_set_app_state(bool in_app, input_callback_t input_cb);
void menus_module_set_default_input(void);
void menus_module_return(void);
