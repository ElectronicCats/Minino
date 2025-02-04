#include "ssid_spam_screens.h"

#include "bitmaps_general.h"
#include "general_flash_storage.h"
#include "oled_screen.h"

void ssid_spam_animation() {
  static uint8_t idx = 0;
  oled_screen_display_bitmap(punch_animation[idx], 48, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > (2 - 1) ? 0 : idx;
}

void ssid_spam_screens_running() {
  oled_screen_clear();
  oled_screen_display_text_center("Spamming", 0, OLED_DISPLAY_NORMAL);
}