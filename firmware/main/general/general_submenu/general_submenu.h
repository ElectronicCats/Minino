#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "general_screens.h"

typedef void (*submenu_selection_handler_t)(uint8_t);

typedef void (*submenu_exit_handler_t)(void);

typedef struct {
  uint8_t options_count;
  const char** options;
  uint8_t selected_option;
  submenu_selection_handler_t select_cb;
  submenu_exit_handler_t exit_cb;
  bool modal;
  char* modal_title;
} general_submenu_menu_t;

void general_submenu(general_submenu_menu_t submenu);