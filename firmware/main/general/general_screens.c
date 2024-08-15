#include "general/general_screens.h"
#include "general/bitmaps_general.h"
#include "oled_screen.h"

#define ITEMOFFSET 2
static const general_menu_t* current_menu_ctx = NULL;

static void general_screen_display_selected_item(char* item_text,
                                                 uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 0, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 16, item_number, OLED_DISPLAY_INVERT);
}

static void general_screen_display_breadcrumb() {
  if (current_menu_ctx->menu_level == 0) {
    oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  }
}

void general_register_menu(const general_menu_t* ctx) {
  current_menu_ctx = ctx;
}

void general_clear_screen() {
  oled_screen_clear();
}

void genera_screen_display_card_information(char* title, char* body) {
  general_clear_screen();
  general_screen_display_breadcrumb();
  oled_screen_display_card_border();
  genera_screen_display_notify_information(title, body);
}

void genera_screen_display_notify_information(char* title, char* body) {
  general_clear_screen();
  general_screen_display_breadcrumb();
  int page = 2;
  oled_screen_display_text_center(title, page, OLED_DISPLAY_NORMAL);
  page++;
  if (strlen(body) > MAX_LINE_CHAR) {
    oled_screen_display_text_splited(body, &page, OLED_DISPLAY_NORMAL);
    return;
  }
  oled_screen_display_text_center(body, page, OLED_DISPLAY_NORMAL);
}

void general_screen_display_menu(uint16_t current_option) {
  general_clear_screen();
  general_screen_display_breadcrumb();

  if (current_menu_ctx == NULL) {
    return;
  }

  for (uint16_t i = 0; i < current_menu_ctx->menu_count; i++) {
    if (i == current_option) {
      general_screen_display_selected_item(current_menu_ctx->menu_items[i],
                                           i + ITEMOFFSET);
    } else {
      oled_screen_display_text(current_menu_ctx->menu_items[i], 0,
                               i + ITEMOFFSET, OLED_DISPLAY_NORMAL);
    }
  }
}