#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int8_t selected_option;
  bool consumed;
  uint8_t options_count;
  char** options;
} modal_get_user_selection_t;

int8_t modal_module_get_user_selection(uint8_t options_count, char** options);
void set_get_user_selection_handler(void* cb);