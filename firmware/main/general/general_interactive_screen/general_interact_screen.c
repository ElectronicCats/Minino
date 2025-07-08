#include "general_interact_screen.h"
#include "oled_screen.h"

static general_interactive_screen_t interactive_screen_ctx = {0};

static void interactive_screen_show() {
  general_interactive_screen_t* ctx = interactive_screen_ctx;
  if (!ctx || !ctx->options || ctx->options_count == 0) {
    oled_screen_clear_buffer();
    oled_screen_display_text("Error: No options", 0, 0, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }
  oled_screen_clear_buffer();

  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Campo 1", 0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

void interactive_screen(general_interactive_screen_t screen) {
  interactive_screen_ctx = calloc(1, sizeof(general_interactive_screen_t));
  interactive_screen_ctx->options = screen.options;
  interactive_screen_ctx->options_count = screen.interactive_screen_ctx;

  interactive_screen_show();
}