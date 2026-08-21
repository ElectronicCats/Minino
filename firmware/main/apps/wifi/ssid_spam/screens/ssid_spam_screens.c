#include "ssid_spam_screens.h"

#include "bitmaps_general.h"
#include "general_flash_storage.h"
#include "oled_screen.h"

void ssid_spam_animation() {
  oled_screen_clear_buffer();
  static uint8_t idx = 0;
#ifdef CONFIG_RESOLUTION_128X64
  oled_screen_display_text_center("Spamming", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_bitmap(punch_animation[idx], 48, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
#else
  oled_screen_display_bitmap(punch_animation[idx], 48, 0, 32, 32,
                             OLED_DISPLAY_NORMAL);
#endif
  idx = (idx + 1) % (sizeof(punch_animation) / sizeof(punch_animation[0]));
  oled_screen_display_show();
}

void ssid_spam_screens_running() {
  oled_screen_clear();
}