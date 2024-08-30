#include <string.h>
#include "oled_screen.h"

void open_thread_screens_display_broadcast_mode(uint8_t ch) {
  oled_screen_clear();
  oled_screen_display_text(" BroadCast Mode ", 0, 0, OLED_DISPLAY_NORMAL);
  char* str = (char*) malloc(18);
  sprintf(str, "  Channel %d    ", ch);
  oled_screen_display_text(str, 0, 7, OLED_DISPLAY_NORMAL);
  free(str);
}

void open_thread_screens_show_new_message(char* msg) {
  oled_screen_clear_line(0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("   New Message  ", 0, 2, OLED_DISPLAY_INVERT);
  char* str = (char*) malloc(18);
  sprintf(str, "%s", msg);
  oled_screen_clear_line(0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(str, 4, OLED_DISPLAY_NORMAL);
  free(str);
}

void open_thread_screens_display_sniffer_animation() {}
