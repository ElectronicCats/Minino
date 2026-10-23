#pragma once
#include <stdbool.h>

typedef void (*ctrl_c_callback_t)(void);
void cat_console_register_ctrl_c_handler(ctrl_c_callback_t callback);
