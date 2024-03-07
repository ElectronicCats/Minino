#ifndef DISPLAY_H
#define DISPLAY_H

#include "sh1106.h"

#define INVERT 1
#define NO_INVERT 0

enum MenuLayer {
    LAYER_MAIN_MENU = 0,
};

void display_init(void);
void display_clear(void);
void display_text(const char* text, int text_size, int page, int invert);
void display_selected_item_box();
void display_menu(uint8_t button_name, uint8_t button_event);

#endif // DISPLAY_H