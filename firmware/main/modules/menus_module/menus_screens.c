#include "menus_screens.h"

#include "oled_screen.h"

#define MAX_MENUS_ON_SCREEN 8

void menus_screens_display_menus(menus_manager_t* ctx) {
  static uint8_t items_offset = 0;
  items_offset =
      MAX(ctx->selected_menu - (MAX_MENUS_ON_SCREEN - 1), items_offset);
  items_offset =
      MIN(MAX(ctx->submenus_count - MAX_MENUS_ON_SCREEN, 0), items_offset);
  items_offset = MIN(ctx->selected_menu, items_offset);
  oled_screen_clear();
  for (uint8_t i = 0; i < (MIN(ctx->submenus_count, MAX_MENUS_ON_SCREEN));
       i++) {
    char* display_name =
        menus[*ctx->submenus_idx[i + items_offset]].display_name;
    oled_screen_display_text(display_name, 0, i,
                             ctx->selected_menu == i + items_offset);
  }
}
