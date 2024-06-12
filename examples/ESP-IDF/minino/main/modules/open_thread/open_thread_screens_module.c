#include <string.h>
#include "oled_screen.h"

void open_thread_screens_display_broadcast_mode() {
  oled_screen_clear();
  oled_screen_display_text_center("BroadCast Mode", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Channel 11", 3, OLED_DISPLAY_NORMAL);
}

void open_thread_screens_show_new_message(void* msg) {
  const char* str = (const char*) msg;
  oled_screen_display_text_center("New Message", 7, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center(msg, 8, OLED_DISPLAY_NORMAL);
}
