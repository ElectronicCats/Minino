#ifndef DISPLAY_H
#define DISPLAY_H

#include "sh1106.h"
#include "display_helper.h"

#define INVERT 1
#define NO_INVERT 0

extern uint8_t selected_option;
extern Layer previous_layer;
extern Layer current_layer;
extern int options_length;

void display_init(void);
void display_clear(void);
void display_text(const char* text, int text_size, int page, int invert);
void display_selected_item_box();
char** add_empty_strings(char** array, int length);
char** get_menu_items();
void display_menu();

#endif // DISPLAY_H