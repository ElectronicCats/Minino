#ifndef __GENERA_INTERACT_SCREEN_H
#define __GENERA_INTERACT_SCREEN_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef void (*screen_up_selection_handler_t)(uint8_t);
typedef void (*screen_down_selection_handler_t)(uint8_t);
typedef void (*screen_select_selection_handler_t)(uint8_t);
typedef void (*screen_on_dinamic_change_handler_t)(uint8_t);

typedef void (*screen_back_selection_handler_t)(void);

typedef struct {
  char* static_text;
  char* dinamic_text;
  char* header_title;
  uint16_t range_low;
  uint16_t range_high;
  uint16_t selected_value;
  uint16_t* dinamic_value;
  screen_up_selection_handler_t select_up_cb;
  screen_down_selection_handler_t select_down_cb;
  screen_back_selection_handler_t select_back_cb;
  screen_select_selection_handler_t select_select_cb;
} general_interactive_screen_t;

void interactive_screen(general_interactive_screen_t screen);
void update_interactive_screen();
#endif