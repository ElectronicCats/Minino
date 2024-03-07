#ifndef DISPLAY_H
#define DISPLAY_H

#include "sh1106.h"

#define INVERT 1
#define NO_INVERT 0

void display_init(void);
void display_clear(void);
void display_text(const char* text, int text_size, int page, int invert);
void display_selected_item_box();
char** add_empty_strings(char** array, int length);
char** get_menu_items();
void display_menu(uint8_t button_name, uint8_t button_event);

#endif // DISPLAY_H