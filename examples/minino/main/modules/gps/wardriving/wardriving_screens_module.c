#include <stdio.h>

#include "gps_bitmaps.h"
#include "oled_screen.h"
#include "wardriving_screens_module.h"

void wardriving_screens_module_scanning(uint32_t packets, char* signal) {
  char* packets_str = (char*) malloc(20);
  sprintf(packets_str, "%ld", packets);

  oled_screen_clear();
  oled_screen_display_text("Packets", 64, 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text(packets_str, 64, 1, OLED_DISPLAY_INVERT);
  oled_screen_display_text("Signal", 64, 3, OLED_DISPLAY_INVERT);
  oled_screen_display_text(signal, 64, 4, OLED_DISPLAY_INVERT);
}

void wardriving_screens_module_loading_text() {
  oled_screen_clear();
  oled_screen_display_text_center("Loading...", 3, OLED_DISPLAY_NORMAL);
}

void wardriving_screens_module_no_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("No SD Card", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("detected!", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}

void wardriving_screens_module_no_gps_signal() {
  static uint8_t index = 0;
  index++;
  if (index == radio_antenna_list_size) {
    index = 0;
  }
  oled_screen_clear();
  oled_screen_display_text_center("Looking for GPS", 5, OLED_DISPLAY_NORMAL);
  uint8_t x = 48;
  uint8_t y = 0;
  oled_screen_display_bitmap(radio_antenna_list[index], x, y, 32, 32,
                             OLED_DISPLAY_NORMAL);
}
