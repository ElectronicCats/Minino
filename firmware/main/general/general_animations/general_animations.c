#include "general_animations.h"
#include "oled_screen.h"

static uint8_t idx = 0;

void general_animation_loading() {
  oled_screen_clear_buffer();
#ifdef CONFIG_RESOLUTION_128X32
  oled_screen_display_bitmap(epd_bitmap_loading_32[idx], 8, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Loading", 48, 2, OLED_DISPLAY_NORMAL);
#else
  oled_screen_display_bitmap(epd_bitmap_loading_32[idx], 48, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Loading", 6, OLED_DISPLAY_NORMAL);
#endif
  idx = (idx + 1) % 4;
  oled_screen_display_show();
}
