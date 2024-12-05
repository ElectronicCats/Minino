#include "ssid_spam_screens.h"

#include "oled_screen.h"

void ssid_spam_screens_running() {
  oled_screen_clear();
  oled_screen_display_text_center("Spamming", 2, OLED_DISPLAY_NORMAL);
}