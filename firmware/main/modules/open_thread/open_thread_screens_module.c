#include <string.h>
#include "oled_screen.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define MAX_ITEMS_NUM 8
  #define ITEMOFFSET    1
#else  // CONFIG_RESOLUTION_128X32
  #define MAX_ITEMS_NUM 4
  #define ITEMOFFSET    1
#endif

void open_thread_screens_display_broadcast_mode(uint8_t ch) {
  oled_screen_clear_buffer();
  oled_screen_display_text(" BroadCast Mode ", 0, 0, OLED_DISPLAY_NORMAL);
  char* str = (char*) malloc(18);
  sprintf(str, "  Channel %d    ", ch);
  oled_screen_display_text(str, 0, MAX_ITEMS_NUM - 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
  free(str);
}

void open_thread_screens_show_new_message(char* msg) {
  oled_screen_clear_line(0, MAX_ITEMS_NUM / 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("   New Message  ", 0, MAX_ITEMS_NUM / 4,
                           OLED_DISPLAY_INVERT);
  char* str = (char*) malloc(18);
  sprintf(str, "%s", msg);
  oled_screen_clear_line(0, MAX_ITEMS_NUM / 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(str, MAX_ITEMS_NUM / 2, OLED_DISPLAY_NORMAL);
  free(str);
}

void open_thread_screens_display_sniffer_animation() {}
