#include <stdio.h>

#include "esp_log.h"

#include "general_screens.h"
#include "gps_bitmaps.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "wardriving_screens_module.h"
#include "wifi_bitmaps.h"

char* wardriving_help_2[] = {
    "This tool",       "allows you to",  "scan for WiFi",
    "networks and",    "save the",       "results in a",
    "CSV file on",     "the SD card.",   "",
    "Before starting", "the scan, make", "sure your date",
    "and time are",    "correct.",
};
const general_menu_t wardriving_help_menu = {
    .menu_count = 14,
    .menu_items = wardriving_help_2,
    .menu_level = GENERAL_TREE_APP_MENU};

void wardriving_screens_show_help() {
  general_register_scrolling_menu(&wardriving_help_menu);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}

void wardriving_screens_wifi_animation_task() {
  oled_screen_clear_buffer();

  while (true) {
    static uint8_t idx = 0;
    oled_screen_display_bitmap(epd_bitmap_wifi_loading[idx], 0, 0, 56, 56,
                               OLED_DISPLAY_NORMAL);
    idx = ++idx > 3 ? 0 : idx;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void wardriving_screens_module_scanning(uint32_t packets, char* signal) {
  char* packets_str = (char*) malloc(20);
  sprintf(packets_str, "%ld", packets);

  // oled_screen_clear_buffer();
  uint8_t x = 64;
  static uint8_t y = 0;

  static uint8_t count = 0;
  count++;
  if (count == 255) {
    uint8_t pages = oled_screen_get_pages();
    for (uint8_t page = 0; page < pages; page++) {
      oled_screen_clear_line(x, page, OLED_DISPLAY_NORMAL);
    }
    y++;
  }

  if (y == 2) {
    y = 0;
  }

  uint8_t yy = y;
  oled_screen_display_text("Packets", x, yy++, OLED_DISPLAY_INVERT);
  oled_screen_display_text(packets_str, x, yy++, OLED_DISPLAY_INVERT);
  yy++;
  if (strcmp(signal, "None") != 1) {
    oled_screen_display_text("GPS", x, yy++, OLED_DISPLAY_INVERT);
    oled_screen_display_text(signal, x, yy++, OLED_DISPLAY_INVERT);
  }
  oled_screen_display_show();
  free(packets_str);
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

void wardriving_screens_module_format_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("SD Card is not", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("in FAT32,", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Format?", 3, OLED_DISPLAY_NORMAL);
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

void wardriving_screens_module_formating_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Formating SD", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Card... don't", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("remove it!", 4, OLED_DISPLAY_NORMAL);
}

void wardriving_screens_module_failed_format_sd_card() {
  oled_screen_clear();
  oled_screen_display_text_center("Failed to", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("format SD Card", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Ok", 5, OLED_DISPLAY_INVERT);
}
