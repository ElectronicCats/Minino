#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "general_screens.h"

typedef void (*radio_selection_handler_t)(uint8_t);
typedef void (*radio_selection_exit_callback_t)(void);

typedef enum {
  RADIO_SELECTION_OLD_STYLE,
  RADIO_SELECTION_NEW_STYLE
} radio_selection_style_t;

typedef struct {
  uint8_t selected_option;
  uint8_t options_count;
  const char** options;
  char* banner;
  uint8_t current_option;
  radio_selection_handler_t select_cb;
  radio_selection_exit_callback_t exit_cb;
} general_radio_selection_t;

typedef struct {
  const char** options;
  char* banner;
  uint8_t current_option;
  uint8_t options_count;
  radio_selection_handler_t select_cb;
  radio_selection_exit_callback_t exit_cb;
  radio_selection_style_t style;
} general_radio_selection_menu_t;

void general_radio_selection(
    general_radio_selection_menu_t radio_selection_menu);