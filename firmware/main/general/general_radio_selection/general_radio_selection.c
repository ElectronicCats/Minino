#include "general_radio_selection.h"

#include <stdlib.h>
#include <string.h>
#include "bitmaps_general.h"
#include "buzzer.h"
#include "esp_log.h"
#include "menus_module.h"
#include "oled_screen.h"

static const char* TAG = "radio_selection";

#ifdef CONFIG_RESOLUTION_128X64
  #define MAX_OPTIONS_NUM 8
  #define ITEMOFFSET      1
#else  // CONFIG_RESOLUTION_128X32
  #define MAX_OPTIONS_NUM 4
  #define ITEMOFFSET      1
#endif

static const uint32_t SOUND_DURATION = 100;
static general_radio_selection_t* general_radio_selection_ctx;

static void list_radio_options_old_style();
static void list_radio_options_new_style();

static void* list_radio_options_styles[] = {list_radio_options_old_style,
                                            list_radio_options_new_style};
static void (*list_radio_options)() = NULL;

/* Room for the row prefix ("[x] "), the option text and the "[curr]" suffix.
   The previous code used a fixed malloc(20) with an unbounded sprintf(), so any
   option of 17 characters or more wrote past the end of the allocation - the
   Time-Zone selector alone has 37 entries, most of them that long. */
static size_t longest_option(const general_radio_selection_t* ctx) {
  size_t longest = 0;
  if (ctx == NULL || ctx->options == NULL) {
    return 0;
  }
  for (size_t i = 0; i < ctx->options_count; i++) {
    if (ctx->options[i] != NULL) {
      size_t n = strlen(ctx->options[i]);
      if (n > longest) {
        longest = n;
      }
    }
  }
  return longest;
}

/* Returns the allocated row buffer, or NULL on OOM. The caller frees it. */
static char* row_buffer_alloc(const general_radio_selection_t* ctx) {
  return (char*) malloc(longest_option(ctx) + 16);
}

/* The window can only show a fixed number of rows; without this bound
   `i + items_offset` walked off the end of options[] (the Time-Zone selector
   with selected_option == 36 and 37 entries reached index 37) and the garbage
   pointer that followed was then dereferenced by sprintf("%s"). */
static uint8_t first_visible_option(const general_radio_selection_t* ctx,
                                    uint8_t items_offset) {
  if (ctx->options_count == 0) {
    return 0;
  }
  uint8_t window = MIN(ctx->options_count, MAX_OPTIONS_NUM - 1);
  uint8_t last = ctx->options_count - 1;
  uint8_t first = (ctx->selected_option >= window)
                      ? (uint8_t) (ctx->selected_option - window + 1)
                      : 0;
  first = MIN(first, last);
  first = MAX(first, items_offset);
  if ((uint32_t) first + window > ctx->options_count) {
    first = (ctx->options_count > window)
                ? (uint8_t) (ctx->options_count - window)
                : 0;
  }
  return first;
}

static void list_radio_options_old_style() {
  general_radio_selection_t* ctx = general_radio_selection_ctx;
  if (ctx == NULL) {
    return;
  }
  static uint8_t items_offset = 0;
  uint8_t first = first_visible_option(ctx, items_offset);
  items_offset = first;

  char* str = row_buffer_alloc(ctx);
  if (!str) {
    ESP_LOGE(TAG, "Out of memory");
    return;
  }
  oled_screen_clear_buffer();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  uint8_t window = MIN(ctx->options_count, MAX_OPTIONS_NUM - 1);
  for (uint8_t i = 0; i < window; i++) {
    uint8_t idx = (uint8_t) (first + i);
    if (idx >= ctx->options_count) {
      break;
    }
    bool is_selected = idx == ctx->selected_option;
    bool is_current = idx == ctx->current_option;
    char state = is_current ? 'x' : ' ';
    const char* label = ctx->options[idx] ? ctx->options[idx] : "";
    snprintf(str, strlen(label) + 5, "[%c] %s", state, label);
    oled_screen_display_text(str, 0, i + ITEMOFFSET, is_selected);
  }
  oled_screen_display_show();
  free(str);
}

static void list_radio_options_new_style() {
  general_radio_selection_t* ctx = general_radio_selection_ctx;
  if (ctx == NULL) {
    return;
  }
  static uint8_t items_offset = 0;
  uint8_t first = first_visible_option(ctx, items_offset);
  items_offset = first;

  char* str = row_buffer_alloc(ctx);
  if (!str) {
    ESP_LOGE(TAG, "Out of memory");
    return;
  }
  oled_screen_clear_buffer();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_bitmap(minino_face, 0, (ctx->selected_option + 2) * 8, 8,
                             8, OLED_DISPLAY_NORMAL);
  uint8_t window = MIN(ctx->options_count, MAX_OPTIONS_NUM - 1);
  for (uint8_t i = 0; i < window; i++) {
    uint8_t idx = (uint8_t) (first + i);
    if (idx >= ctx->options_count) {
      break;
    }
    bool is_selected = idx == ctx->selected_option;
    bool is_current = idx == ctx->current_option;
    const char* label = ctx->options[idx] ? ctx->options[idx] : "";
    snprintf(str, strlen(label) + 7, "%s%s", label, is_current ? "[curr]" : "");
    oled_screen_display_text(str, is_selected ? 16 : 0, i + 2, is_selected);
  }
  oled_screen_display_show();
  free(str);
}

static void input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN ||
      general_radio_selection_ctx == NULL) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      void (*exit_cb)() = general_radio_selection_ctx->exit_cb;
      free(general_radio_selection_ctx);
      general_radio_selection_ctx = NULL;
      if (exit_cb) {
        exit_cb();
      }
      break;
    case BUTTON_RIGHT:
      general_radio_selection_ctx->current_option =
          general_radio_selection_ctx->selected_option;
      radio_selection_handler_t select_cb =
          general_radio_selection_ctx->select_cb;
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
  if (general_radio_selection_ctx) {
    free(general_radio_selection_ctx);
    general_radio_selection_ctx = NULL;
  }
  if (radio_selection_menu.options == NULL ||
      radio_selection_menu.options_count == 0) {
    ESP_LOGE(TAG, "selector opened with no options");
    return;
  }
  general_radio_selection_ctx = calloc(1, sizeof(general_radio_selection_t));
  if (general_radio_selection_ctx == NULL) {
    ESP_LOGE(TAG, "Out of memory");
    return;
  }
  general_radio_selection_ctx->options = radio_selection_menu.options;
  general_radio_selection_ctx->options_count =
      radio_selection_menu.options_count;
  general_radio_selection_ctx->banner = radio_selection_menu.banner;
  /* current_option comes from the caller and may exceed the option list (a
     stale saved selection, or a different menu), which would index past
     options[] in the renderers. */
  uint16_t current = radio_selection_menu.current_option;
  if (current >= radio_selection_menu.options_count) {
    current = 0;
  }
  general_radio_selection_ctx->current_option = current;
  general_radio_selection_ctx->selected_option = current;
  general_radio_selection_ctx->select_cb = radio_selection_menu.select_cb;
  general_radio_selection_ctx->exit_cb = radio_selection_menu.exit_cb;
  menus_module_set_app_state(true, input_cb);
  size_t styles =
      sizeof(list_radio_options_styles) / sizeof(list_radio_options_styles[0]);
  size_t style = radio_selection_menu.style;
  if (style >= styles) {
    style = 0;
  }
  list_radio_options = list_radio_options_styles[style];
  list_radio_options();
}