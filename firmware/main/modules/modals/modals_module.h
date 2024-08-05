#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int8_t selected_option;
  bool consumed;
  uint8_t options_count;
  char** options;
  char* banner;
} modal_get_user_selection_t;

int8_t modal_module_get_user_selection(char** options, char* banner);
void set_get_user_selection_handler(void* cb);