#include "general/general_screens.h"
#include "general/bitmaps_general.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

#define MAX_LINE_CHAR 16

#ifdef CONFIG_RESOLUTION_128X64
  #define ITEMOFFSET           2
  #define ITEM_PAGE_OFFSET     2
  #define MAX_ITEMS_PER_SCREEN 5
#else  // CONFIG_RESOLUTION_128X32
  #define ITEMOFFSET           1
  #define ITEM_PAGE_OFFSET     1
  #define MAX_ITEMS_PER_SCREEN 3
#endif
static uint8_t scrolling_option = 0;
static const general_menu_t* current_menu_ctx = NULL;
static const general_menu_t* scrolling_menu_ctx = NULL;
static void* (*menu_exit_function)(void);
static void* (*menu_restore_function)(void);
static void general_screen_display_scrolling();
static void general_screen_cb_modal(uint8_t button_name, uint8_t button_event);
static void general_screen_cb_scroll(uint8_t button_name, uint8_t button_event);
static const general_menu_t card_info_menu_ctx = {
    .menu_items = NULL,
    .menu_count = 0,
    .menu_level = GENERAL_TREE_APP_SUBMENU,
};

void general_screen_truncate_text(char* p_text, char* p_truncated_text) {
  // Truncate the text if it is longer than the screen width and add 3 dots
  char* p_truncated_text_ptr = p_truncated_text;
  for (uint8_t i = 0; i < (MAX_LINE_CHAR - 3); i++) {
    *p_truncated_text_ptr = *p_text;
    p_text++;
    p_truncated_text_ptr++;
  }
  *p_truncated_text_ptr++ = '.';
  *p_truncated_text_ptr++ = '.';
  *p_truncated_text_ptr++ = '.';
  *p_truncated_text_ptr = '\0';
}

static void general_screen_display_selected_item(char* item_text,
                                                 uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 0, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 16, item_number, OLED_DISPLAY_INVERT);
}

static void general_screen_increment_option() {
  scrolling_option++;
  if (scrolling_option >= scrolling_menu_ctx->menu_count) {
    scrolling_option = 0;
  }
}

static void general_screen_decrement_option() {
  scrolling_option = scrolling_option-- == 0
                         ? scrolling_menu_ctx->menu_count - 1
                         : scrolling_option;
}

static void general_screen_display_breadcrumb() {
  if (current_menu_ctx == NULL) {
    oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
    return;
  }
  if (current_menu_ctx->menu_level == GENERAL_TREE_APP_MENU) {
    oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  }
}

static void general_screen_cb_modal(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_LEFT:
      menus_module_set_app_state_last();
      menu_exit_function();
      break;
    default:
      break;
  }
}

static void general_screen_cb_scroll(uint8_t button_name,
                                     uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      general_screen_decrement_option();
      general_screen_display_scrolling();
      break;
    case BUTTON_DOWN:
      general_screen_increment_option();
      general_screen_display_scrolling();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_LEFT:
      general_register_menu(current_menu_ctx);
      general_register_scrolling_menu(NULL);
      menus_module_set_app_state_last();
      menu_exit_function();
      break;
    default:
      break;
  }
}

static void general_screen_display_scrolling() {
  general_clear_screen();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);

  if (scrolling_menu_ctx == NULL) {
    return;
  }

  oled_screen_display_card_border();
#ifdef CONFIG_RESOLUTION_128X64
  uint16_t items_per_screen = 3;
  uint16_t screen_title = 2;

  oled_screen_display_text_center("Information", ITEM_PAGE_OFFSET,
                                  OLED_DISPLAY_NORMAL);
#else
  uint16_t items_per_screen = 2;
  uint16_t screen_title = 0;
#endif

  uint16_t end_index = scrolling_option + items_per_screen;
  if (end_index > scrolling_menu_ctx->menu_count) {
    end_index = scrolling_menu_ctx->menu_count;
  }

  for (uint16_t i = scrolling_option; i < end_index; i++) {
    oled_screen_display_text(
        scrolling_menu_ctx->menu_items[i], 3,
        (i - scrolling_option) + (ITEMOFFSET + screen_title),
        OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}

void general_register_menu(const general_menu_t* ctx) {
  current_menu_ctx = ctx;
}

void general_register_scrolling_menu(const general_menu_t* ctx) {
  scrolling_menu_ctx = ctx;
}

void general_clear_screen() {
  oled_screen_clear_buffer();
}

void general_screen_display_scrolling_text_handler(void* callback_exit) {
  scrolling_option = 0;
  menu_exit_function = callback_exit;
  menus_module_set_app_state(true, general_screen_cb_scroll);
  general_screen_display_scrolling();
}

void general_screen_display_card_information_handler(char* title,
                                                     char* body,
                                                     void* callback_exit,
                                                     void* callback_restore) {
  menu_exit_function = callback_exit;
  menu_restore_function = callback_restore;
  menus_module_set_app_state(true, general_screen_cb_modal);
  genera_screen_display_card_information(title, body);
}

void genera_screen_display_card_information(char* title, char* body) {
  general_register_menu(&card_info_menu_ctx);
  general_clear_screen();
  general_screen_display_breadcrumb();
  oled_screen_display_card_border();
  int page = ITEM_PAGE_OFFSET;
  oled_screen_display_text_center(title, page, OLED_DISPLAY_NORMAL);
  page++;
  if (strlen(body) > MAX_LINE_CHAR) {
    oled_screen_display_text_splited(body, &page, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }
  oled_screen_display_text_center(body, page, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

void genera_screen_display_notify_information(char* title, char* body) {
  general_clear_screen();
  general_screen_display_breadcrumb();
  int page = ITEM_PAGE_OFFSET;
  oled_screen_display_card_border();
  oled_screen_display_text_center(title, page, OLED_DISPLAY_NORMAL);
  page++;
  if (strlen(body) > MAX_LINE_CHAR) {
    oled_screen_display_text_splited(body, &page, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }
  oled_screen_display_text_center(body, page, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

void general_screen_display_menu(uint16_t current_option) {
  general_clear_screen();
  general_screen_display_breadcrumb();

  if (current_menu_ctx == NULL) {
    return;
  }

  if (current_option >= current_menu_ctx->menu_count) {
    current_option = 0;
  }

  const uint16_t MAX_VISIBLE_ITEMS = 6;

  uint16_t start_index = current_option - (current_option % MAX_VISIBLE_ITEMS);
  if (current_menu_ctx->menu_count <= MAX_VISIBLE_ITEMS) {
    start_index = 0;  // No need to scroll if all items fit
  } else if (start_index + MAX_VISIBLE_ITEMS > current_menu_ctx->menu_count) {
    start_index = current_menu_ctx->menu_count - MAX_VISIBLE_ITEMS;
  }

  for (uint16_t i = 0; i < MAX_VISIBLE_ITEMS &&
                       (start_index + i) < current_menu_ctx->menu_count;
       i++) {
    uint16_t item_index = start_index + i;
    if (item_index == current_option) {
      general_screen_display_selected_item(
          current_menu_ctx->menu_items[item_index], i + ITEMOFFSET);
    } else {
      oled_screen_display_text(current_menu_ctx->menu_items[item_index], 0,
                               i + ITEMOFFSET, OLED_DISPLAY_NORMAL);
    }
  }

  oled_screen_display_show();
}
