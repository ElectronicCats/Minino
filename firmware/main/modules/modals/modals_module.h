#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum { CANCELED_OPTION = -1, NO_OPTION, YES_OPTION } yes_no_options_t;

typedef struct {
  int8_t selected_option;
  bool consumed;
  uint8_t options_count;
  char** options;
  char* banner;
} modal_get_user_selection_t;

int8_t modals_module_get_user_selection(char** options, char* banner);
void set_get_user_selection_handler(void* cb);
void modals_module_show_info(char* head, char* body, size_t time_ms);
bool modals_module_get_user_y_n_selection(char* banner);