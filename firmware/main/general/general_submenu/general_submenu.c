#include "general_submenu.h"

#include "bitmaps_general.h"
#include "menus_module.h"
#include "oled_screen.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define MAX_OPTIONS_NUM 8
#else  // CONFIG_RESOLUTION_128X32
  #define MAX_OPTIONS_NUM 4
#endif

static general_submenu_menu_t* general_radio_selection_ctx;

static void list_submenu_options() {
  general_submenu_menu_t* ctx = general_radio_selection_ctx;
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - MAX_OPTIONS_NUM + 2, items_offset);
  items_offset =
      MIN(MAX(ctx->options_count - MAX_OPTIONS_NUM + 2, 0), items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear_buffer();
  char* str = malloc(20);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    bool is_selected = i + items_offset == ctx->selected_option;
    sprintf(str, "%s", ctx->options[i + items_offset]);
    oled_screen_display_text(str, 0, i + 1, is_selected);
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
      void (*exit_cb)() = general_radio_selection_ctx->exit_cb;
      free(general_radio_selection_ctx);
      if (exit_cb) {
        exit_cb();
      }
      break;
    case BUTTON_RIGHT:
      void (*select_cb)() = general_radio_selection_ctx->select_cb;
      if (select_cb) {
        select_cb(general_radio_selection_ctx->selected_option);
      }
      break;
    case BUTTON_UP:
      general_radio_selection_ctx->selected_option =
          general_radio_selection_ctx->selected_option == 0
              ? general_radio_selection_ctx->options_count - 1
              : general_radio_selection_ctx->selected_option - 1;
      list_submenu_options();
      break;
    case BUTTON_DOWN:
      general_radio_selection_ctx->selected_option =
          ++general_radio_selection_ctx->selected_option <
                  general_radio_selection_ctx->options_count
              ? general_radio_selection_ctx->selected_option
              : 0;
      list_submenu_options();
      break;
    default:
      break;
  }
}

void general_submenu(general_submenu_menu_t radio_selection_menu) {
  general_radio_selection_ctx = calloc(1, sizeof(general_submenu_menu_t));
  general_radio_selection_ctx->options = radio_selection_menu.options;
  general_radio_selection_ctx->options_count =
      radio_selection_menu.options_count;
  general_radio_selection_ctx->select_cb = radio_selection_menu.select_cb;
  general_radio_selection_ctx->exit_cb = radio_selection_menu.exit_cb;
  menus_module_set_app_state(true, input_cb);
  list_submenu_options();
}