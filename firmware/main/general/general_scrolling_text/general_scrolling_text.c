#include "general_scrolling_text.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bitmaps_general.h"
#include "esp_log.h"
#include "general_scroll_split.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "oled_screen.h"

general_scrolling_text_ctx* scroll_text_ctx;

static void scroll_text_draw_window() {
  if (scroll_text_ctx == NULL) {
    return;
  }
  oled_screen_clear_buffer();
  if (scroll_text_ctx->banner) {
    oled_screen_display_text(scroll_text_ctx->banner, 0, 0,
                             OLED_DISPLAY_NORMAL);
  }

  oled_screen_display_card_border();
#ifdef CONFIG_RESOLUTION_128X64
  uint16_t items_per_screen = 3;
  uint16_t screen_title = 2;
#else
  uint16_t items_per_screen = 2;
  uint16_t screen_title = 0;
#endif

  uint16_t end_index = scroll_text_ctx->current_idx + items_per_screen;
  if (end_index > scroll_text_ctx->text_len) {
    end_index = scroll_text_ctx->text_len;
  }

  if (!(scroll_text_ctx->scroll_type == GENERAL_SCROLLING_TEXT_CLAMPED &&
        !scroll_text_ctx->current_idx)) {
    oled_screen_display_bitmap(simple_up_arrow_bmp, 118, 28, 8, 8,
                               OLED_DISPLAY_NORMAL);
  }
  if (!(scroll_text_ctx->scroll_type == GENERAL_SCROLLING_TEXT_CLAMPED &&
        scroll_text_ctx->current_idx >= scroll_text_ctx->text_len - 1)) {
    oled_screen_display_bitmap(simple_down_arrow_bmp, 118, 36, 8, 8,
                               OLED_DISPLAY_NORMAL);
  }

  for (uint16_t i = scroll_text_ctx->current_idx; i < end_index; i++) {
    if (scroll_text_ctx->text_arr[i] != NULL) {
      oled_screen_display_text(
          (char*) scroll_text_ctx->text_arr[i], 3,
          (i - scroll_text_ctx->current_idx) + (1 + screen_title),
          OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
}

static void scroll_text_draw_full_screen() {}

static void scroll_text_draw() {
  if (scroll_text_ctx->window_type == GENERAL_SCROLLING_TEXT_FULL_SCREEN) {
    scroll_text_draw_full_screen();
  } else {
    scroll_text_draw_window();
  }
}

static void scroll_text_ctx_free() {
  if (scroll_text_ctx) {
    if (scroll_text_ctx->text) {
      for (uint16_t i = 0; i < scroll_text_ctx->text_len; ++i) {
        free((void*) scroll_text_ctx->text_arr[i]);
      }
      free(scroll_text_ctx->text_arr);
    }
    free(scroll_text_ctx);
    scroll_text_ctx = NULL;
  }
}

static void button_up_handler() {
  if (scroll_text_ctx->current_idx == 0) {
    if (scroll_text_ctx->scroll_type == GENERAL_SCROLLING_TEXT_INFINITE) {
      scroll_text_ctx->current_idx = scroll_text_ctx->text_len - 1;
    } else {
      return;
    }
  } else {
    scroll_text_ctx->current_idx--;
  }
  scroll_text_draw();
}

static void button_down_handler() {
  if (scroll_text_ctx->current_idx >= scroll_text_ctx->text_len - 1) {
    if (scroll_text_ctx->finish_cb) {
      scroll_text_ctx->finish_cb();
    }
    if (scroll_text_ctx->scroll_type == GENERAL_SCROLLING_TEXT_INFINITE) {
      scroll_text_ctx->current_idx = 0;
    } else {
      return;
    }
  } else {
    scroll_text_ctx->current_idx++;
  }
  scroll_text_draw();
}

static void scrolling_text_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      void (*exit_cb)() = scroll_text_ctx->exit_cb;
      scroll_text_ctx_free();
      if (exit_cb) {
        exit_cb();
      }
      break;
    case BUTTON_RIGHT:
      if (scroll_text_ctx->select_cb) {
        scroll_text_ctx->select_cb();
      }
      break;
    case BUTTON_UP:
      button_up_handler();
      break;
    case BUTTON_DOWN:
      button_down_handler();
      break;
    default:
      break;
  }
}

void general_scrolling_text(general_scrolling_text_ctx ctx) {
  scroll_text_ctx_free();
  scroll_text_ctx = calloc(1, sizeof(general_scrolling_text_ctx));
  if (scroll_text_ctx == NULL) {
    return;
  }
  scroll_text_ctx->banner = ctx.banner;
  scroll_text_ctx->text = ctx.text;
  scroll_text_ctx->window_type = ctx.window_type;
  scroll_text_ctx->scroll_type = ctx.scroll_type;
  scroll_text_ctx->exit_cb = ctx.exit_cb;
  scroll_text_ctx->finish_cb = ctx.finish_cb;
  scroll_text_ctx->text_arr = (const char**) general_scroll_text_split(
      scroll_text_ctx->text, &scroll_text_ctx->text_len);
  if (scroll_text_ctx->text_arr == NULL) {
    /* Leave the UI in the menu rather than installing an input callback over a
       context with no lines: drawing would index text_arr[0] on NULL. */
    ESP_LOGE("scroll_text", "could not lay out the text");
    scroll_text_ctx_free();
    return;
  }

  menus_module_set_app_state(true, scrolling_text_input_cb);
  scroll_text_draw();
}

void general_scrolling_text_array(general_scrolling_text_ctx ctx) {
  scroll_text_ctx_free();
  scroll_text_ctx = calloc(1, sizeof(general_scrolling_text_ctx));
  if (scroll_text_ctx == NULL) {
    return;
  }
  scroll_text_ctx->banner = ctx.banner;
  scroll_text_ctx->window_type = ctx.window_type;
  scroll_text_ctx->scroll_type = ctx.scroll_type;
  scroll_text_ctx->exit_cb = ctx.exit_cb;
  scroll_text_ctx->finish_cb = ctx.finish_cb;
  scroll_text_ctx->text_arr = ctx.text_arr;
  scroll_text_ctx->text_len = ctx.text_len;

  menus_module_set_app_state(true, scrolling_text_input_cb);
  scroll_text_draw();
}

uint16_t general_scrolling_text_get_current_idx() {
  return scroll_text_ctx->current_idx;
}