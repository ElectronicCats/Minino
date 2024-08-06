#include "keyboard_screens.h"

#include "oled_screen.h"
#define TAG "Keyboard Screens"

#define MAX_CHARS 16

void keyboards_screens_update(keyboard_modal_ctx_t* ctx) {
  static uint8_t chars_offset = 0;
  chars_offset = MAX(ctx->current_char - MAX_CHARS - 1, chars_offset);
  chars_offset = MIN(ctx->current_char, chars_offset);
  oled_screen_clear();
  oled_screen_display_text(ctx->banner, 0, 0, OLED_DISPLAY_NORMAL);

  oled_screen_display_text(ctx->new_text, 0, 2, OLED_DISPLAY_NORMAL);
}