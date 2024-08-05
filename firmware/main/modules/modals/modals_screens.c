#include "modals_screens.h"
#include "oled_screen.h"

#define MAX_OPTIONS_NUM 7

void modals_screens_update_options_list(modal_get_user_selection_t* ctx) {
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - 6, items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM)); i++) {
    oled_screen_display_text(ctx->options[i + items_offset], 0, i + 1,
                             ctx->selected_option == i + items_offset);
  }
}