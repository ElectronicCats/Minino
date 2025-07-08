#ifndef __GENERA_INTERACT_SCREEN_H
#define __GENERA_INTERACT_SCREEN_H

typedef void (*screen_up_selection_handler_t)(uint8_t);
typedef void (*screen_down_selection_handler_t)(uint8_t);
typedef void (*screen_select_selection_handler_t)(uint8_t);

typedef void (*screen_back_selection_handler_t)(void);

typedef struct {
  uint8_t options_count;
  char** options;
  uint8_t selected_option;
  char* header_title;
  bool show_breadcrum;
  screen_up_selection_handler_t* select_up_cb;
  screen_down_selection_handler_t* select_down_cb;
  screen_back_selection_handler_t* select_back_cb;
  screen_select_selection_handler_t* select_select_cb;
} general_interactive_screen_t;

void interactive_screen(general_interactive_screen_t screen);

#endif