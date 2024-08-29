#include "general_radio_selection.h"

#include "bitmaps_general.h"
#include "buzzer.h"
#include "menus_module.h"
#include "oled_screen.h"

#define MAX_OPTIONS_NUM 6

static const uint32_t SOUND_DURATION = 100;
static general_radio_selection_t* general_radio_selection_ctx;

static void list_radio_options_old_style();
static void list_radio_options_new_style();

static void* list_radio_options_styles[] = {list_radio_options_old_style,
                                            list_radio_options_new_style};
static void (*list_radio_options)() = NULL;

static void list_radio_options_old_style() {
  general_radio_selection_t* ctx = general_radio_selection_ctx;
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - 4, items_offset);
  items_offset = MIN(MAX(ctx->options_count - 4, 0), items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear_buffer();
  char* str = malloc(20);
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    bool is_selected = i + items_offset == ctx->selected_option;
    bool is_current = i + items_offset == ctx->current_option;
    char state = is_current ? 'x' : ' ';
    sprintf(str, "[%c] %s", state, ctx->options[i + items_offset]);
    oled_screen_display_text(str, 0, i + 2, is_selected);
  }
  oled_screen_display_show();
  free(str);
}

static void list_radio_options_new_style() {
  general_radio_selection_t* ctx = general_radio_selection_ctx;
  static uint8_t items_offset = 0;
  items_offset = MAX(ctx->selected_option - 4, items_offset);
  items_offset = MIN(MAX(ctx->options_count - 4, 0), items_offset);
  items_offset = MIN(ctx->selected_option, items_offset);
  oled_screen_clear_buffer();
  char* str = malloc(20);
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  for (uint8_t i = 0; i < (MIN(ctx->options_count, MAX_OPTIONS_NUM - 1)); i++) {
    bool is_selected = i + items_offset == ctx->selected_option;
    bool is_current = i + items_offset == ctx->current_option;
    oled_screen_display_bitmap(minino_face, 0, (ctx->selected_option + 2) * 8,
                               8, 8, OLED_DISPLAY_NORMAL);
    sprintf(str, "%s%s", ctx->options[i + items_offset],
            is_current ? "[curr]" : "");
    oled_screen_display_text(str, is_selected ? 16 : 0, i + 2, is_selected);
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
      if (exit_cb) {
        exit_cb();
      }
      free(general_radio_selection_ctx);
      break;
    case BUTTON_RIGHT:
      general_radio_selection_ctx->current_option =
          general_radio_selection_ctx->selected_option;
      void (*select_cb)() = general_radio_selection_ctx->select_cb;
      if (select_cb) {
        select_cb(general_radio_selection_ctx->current_option);
      }
      buzzer_play_for(SOUND_DURATION);
      list_radio_options();
      break;
    case BUTTON_UP:
      general_radio_selection_ctx->selected_option =
          general_radio_selection_ctx->selected_option == 0
              ? general_radio_selection_ctx->options_count - 1
              : general_radio_selection_ctx->selected_option - 1;
      list_radio_options();
      break;
    case BUTTON_DOWN:
      general_radio_selection_ctx->selected_option =
          ++general_radio_selection_ctx->selected_option <
                  general_radio_selection_ctx->options_count
              ? general_radio_selection_ctx->selected_option
              : 0;
      list_radio_options();
      break;
    default:
      break;
  }
}

void general_radio_selection(
    general_radio_selection_menu_t radio_selection_menu) {
  general_radio_selection_ctx = calloc(1, sizeof(general_radio_selection_t));
  general_radio_selection_ctx->options = radio_selection_menu.options;
  general_radio_selection_ctx->options_count =
      radio_selection_menu.options_count;
  general_radio_selection_ctx->banner = radio_selection_menu.banner;
  general_radio_selection_ctx->current_option =
      radio_selection_menu.current_option;
  general_radio_selection_ctx->selected_option =
      radio_selection_menu.current_option;
  general_radio_selection_ctx->select_cb = radio_selection_menu.select_cb;
  general_radio_selection_ctx->exit_cb = radio_selection_menu.exit_cb;
  menus_module_set_app_state(true, input_cb);
  list_radio_options = list_radio_options_styles[radio_selection_menu.style];
  list_radio_options();
}