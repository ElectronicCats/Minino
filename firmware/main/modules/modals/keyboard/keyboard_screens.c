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
  for (uint8_t i = 0; i < (MIN(ctx->text_length, MAX_CHARS - 1)); i++) {
    char a_char[2];
    snprintf(a_char, sizeof(a_char), "%c", ctx->new_text[i + chars_offset]);
    oled_screen_display_text(&a_char, i * 8, 2,
                             ctx->current_char == i + chars_offset);
  }
}