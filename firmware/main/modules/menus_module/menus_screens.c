#include "menus_screens.h"

#include <string.h>
#include "oled_screen.h"

#define MAX_MENUS_ON_SCREEN 8

void menus_screens_display_menus(menus_manager_t* ctx) {
  static uint8_t items_offset = 0;
  items_offset =
      MAX(ctx->selected_submenu - (MAX_MENUS_ON_SCREEN - 1), items_offset);
  items_offset =
      MIN(MAX(ctx->submenus_count - MAX_MENUS_ON_SCREEN, 0), items_offset);
  items_offset = MIN(ctx->selected_submenu, items_offset);
  oled_screen_clear();
  for (uint8_t i = 0; i < (MIN(ctx->submenus_count, MAX_MENUS_ON_SCREEN));
       i++) {
    const char* display_name =
        menus[*ctx->submenus_idx[i + items_offset]].display_name;
    oled_screen_display_text(display_name, 0, i,
                             ctx->selected_submenu == i + items_offset);
  }
}
void menus_screens_display_menus_f(menus_manager_t* ctx) {
  oled_screen_clear_buffer();
  if (!ctx->submenus_count) {
    return;
  }
#ifdef CONFIG_RESOLUTION_128X64
  char* prefix = "  ";
  uint8_t page = 1;
  uint8_t page_increment = 2;
#else  // CONFIG_RESOLUTION_128X32
  char* prefix = "> ";
  uint8_t page = 0;
  uint8_t page_increment = 1;
#endif

  bool skip_first = !ctx->selected_submenu;
  bool skip_last = ctx->selected_submenu == ctx->submenus_count - 1;

  uint8_t idx = 0;
  for (uint8_t i = 0; i < 3; i++) {
    if ((!i && skip_first) || (i == 2 && skip_last)) {
      continue;
    }
    char* display_name =
        menus[*ctx->submenus_idx[(idx++) + ctx->selected_submenu - !skip_first]]
            .display_name;
    char* str = (char*) malloc(strlen(display_name) + 3);
    sprintf(str, "%s%s", i == 1 ? prefix : " ", display_name);
    oled_screen_display_text(str, 0, i * page_increment + page,
                             OLED_DISPLAY_NORMAL);
  }

#ifdef CONFIG_RESOLUTION_128X64
  oled_screen_display_selected_item_box();
  oled_screen_display_show();
#endif
}
