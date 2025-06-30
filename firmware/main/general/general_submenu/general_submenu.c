#include "general_submenu.h"

#include "bitmaps_general.h"
#include "menus_module.h"
#include "oled_screen.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define MAX_OPTIONS_NUM 8
#else  // CONFIG_RESOLUTION_128X32
  #define MAX_OPTIONS_NUM 4
#endif
#define MAX_OPTION_LEN 29

static general_submenu_menu_t* submenu_ctx;

static void list_submenu_options_modal() {
  general_submenu_menu_t* ctx = submenu_ctx;
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - MAX_OPTIONS_NUM + 2, items_offset);
  items_offset =
      MIN(MAX(ctx->options_count - MAX_OPTIONS_NUM + 2, 0), items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear_buffer();
  oled_screen_display_card_border();
  uint8_t modal_offset = 3;
  if (ctx->modal_title != NULL) {
    oled_screen_display_text_center(ctx->modal_title, 1, OLED_DISPLAY_NORMAL);
  }
  char* str = malloc(20);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    bool is_selected = i + items_offset == ctx->selected_option;
    sprintf(str, "%s", ctx->options[i + items_offset]);

    if (is_selected) {
      oled_screen_display_bmp_text(minino_face, str, (i + modal_offset), 8, 8,
                                   is_selected);
    } else {
      oled_screen_display_text(str, 8, i + modal_offset, OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
  free(str);
}

static void list_submenu_options() {
  general_submenu_menu_t* ctx = submenu_ctx;
  if (!ctx || !ctx->options || ctx->options_count == 0) {
    oled_screen_clear_buffer();
    oled_screen_display_text("Error: No options", 0, 0, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - MAX_OPTIONS_NUM + 2, items_offset);
  items_offset =
      MIN(MAX(ctx->options_count - MAX_OPTIONS_NUM + 2, 0), items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);

  char* str = (char*) malloc(MAX_OPTION_LEN + 1);
  if (!str) {
    oled_screen_display_show();
    return;
  }
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    uint8_t idx = i + items_offset;
    if (idx >= ctx->options_count || !ctx->options[idx]) {
      printf("Invalid option at index %d\n", idx);
      continue;
    }
    bool is_selected = idx == ctx->selected_option;
    snprintf(str, MAX_OPTION_LEN + 1, "%s", ctx->options[idx]);
    oled_screen_display_text(str, is_selected ? 16 : 0, i + 1, is_selected);
    if (is_selected) {
      oled_screen_display_bitmap(minino_face, 0, (i + 1) * 8, 8, 8,
                                 OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
  free(str);
}

static void input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      void (*exit_cb)() = submenu_ctx->exit_cb;
      free(submenu_ctx);
      if (exit_cb) {
        exit_cb();
      }
      break;
    case BUTTON_RIGHT:
      void (*select_cb)() = submenu_ctx->select_cb;
      if (select_cb) {
        select_cb(submenu_ctx->selected_option);
      }
      break;
    case BUTTON_UP:
      submenu_ctx->selected_option = submenu_ctx->selected_option == 0
                                         ? submenu_ctx->options_count - 1
                                         : submenu_ctx->selected_option - 1;
      if (submenu_ctx->modal) {
        list_submenu_options_modal();
      } else {
        list_submenu_options();
      }
      break;
    case BUTTON_DOWN:
      submenu_ctx->selected_option =
          ++submenu_ctx->selected_option < submenu_ctx->options_count
              ? submenu_ctx->selected_option
              : 0;
      if (submenu_ctx->modal) {
        list_submenu_options_modal();
      } else {
        list_submenu_options();
      }
      break;
    default:
      break;
  }
}

void general_submenu(general_submenu_menu_t submenu) {
  submenu_ctx = calloc(1, sizeof(general_submenu_menu_t));
  submenu_ctx->options = submenu.options;
  submenu_ctx->options_count = submenu.options_count;
  submenu_ctx->select_cb = submenu.select_cb;
  submenu_ctx->exit_cb = submenu.exit_cb;
  submenu_ctx->selected_option = submenu.selected_option;
  submenu_ctx->modal = submenu.modal;
  if (submenu.modal_title != NULL) {
    submenu_ctx->modal_title = submenu.modal_title;
  }
  menus_module_set_app_state(true, input_cb);
  if (submenu.modal != NULL) {
    submenu_ctx->modal = true;
    list_submenu_options_modal();
  } else {
    submenu_ctx->modal = false;
    list_submenu_options();
  }
}