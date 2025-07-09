#include "general_interact_screen.h"
#include "bitmaps_general.h"
#include "esp_log.h"
#include "menus_module.h"
#include "oled_screen.h"

static general_interactive_screen_t* interactive_screen_ctx = {0};
static uint16_t counter = 0;

static void interactive_screen_show() {
  general_interactive_screen_t* ctx = interactive_screen_ctx;
  oled_screen_clear_buffer();

  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ctx->header_title, 1, OLED_DISPLAY_INVERT);
  oled_screen_display_text(ctx->static_text, 0, 2, OLED_DISPLAY_NORMAL);
  char counter_str[20];
  sprintf(counter_str, "%d", counter);
  oled_screen_display_bitmap(simple_up_arrow_bmp, 0, 24, 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_bitmap(simple_down_arrow_bmp, 0, 42, 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(counter_str, 0, 4, OLED_DISPLAY_NORMAL);

  if (ctx->dinamic_text) {
    oled_screen_display_text(ctx->dinamic_text, 64, 2, OLED_DISPLAY_NORMAL);
  }
  if (ctx->dinamic_value) {
    char dinamic_value_str[20];
    sprintf(dinamic_value_str, "%d", *ctx->dinamic_value);
    oled_screen_display_text(dinamic_value_str, 64, 3, OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}

static void input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      void (*exit_cb)() = interactive_screen_ctx->select_back_cb;
      free(interactive_screen_ctx);
      if (exit_cb) {
        exit_cb();
      }
      break;
    case BUTTON_RIGHT:
      void (*select_select_cb)() = interactive_screen_ctx->select_select_cb;
      if (select_select_cb) {
        select_select_cb(counter);
      }
      break;
    case BUTTON_UP:
      counter = counter == interactive_screen_ctx->range_high
                    ? interactive_screen_ctx->range_low
                    : counter + 1;
      void (*select_up_cb)() = interactive_screen_ctx->select_up_cb;
      if (select_up_cb) {
        select_up_cb(counter);
      }
      interactive_screen_show();
      break;
    case BUTTON_DOWN:
      counter = counter == interactive_screen_ctx->range_low
                    ? interactive_screen_ctx->range_high
                    : counter - 1;
      void (*select_down_cb)() = interactive_screen_ctx->select_down_cb;
      if (select_down_cb) {
        select_down_cb(counter);
      }
      interactive_screen_show();
      break;
    default:
      break;
  }
}

void interactive_screen(general_interactive_screen_t screen) {
  interactive_screen_ctx = calloc(1, sizeof(general_interactive_screen_t));
  interactive_screen_ctx->header_title = screen.header_title;
  interactive_screen_ctx->static_text = screen.static_text;
  interactive_screen_ctx->dinamic_text = screen.dinamic_text;
  interactive_screen_ctx->dinamic_value = screen.dinamic_value;
  interactive_screen_ctx->select_back_cb = screen.select_back_cb;
  interactive_screen_ctx->select_up_cb = screen.select_up_cb;
  interactive_screen_ctx->select_select_cb = screen.select_select_cb;
  interactive_screen_ctx->selected_value =
      screen.selected_value ? screen.selected_value : 0;
  interactive_screen_ctx->range_low = screen.range_low ? screen.range_low : 0;
  interactive_screen_ctx->range_high =
      screen.range_high ? screen.range_high : 100;

  counter = interactive_screen_ctx->selected_value;

  menus_module_set_app_state(true, input_cb);
  interactive_screen_show();
}

void update_interactive_screen() {
  interactive_screen_show();
}