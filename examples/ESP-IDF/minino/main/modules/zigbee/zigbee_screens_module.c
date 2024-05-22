#include "zigbee_screens_module.h"
#include "oled_screen.h"
#include "zigbee_bitmaps.h"

void zigbee_screens_module_toogle_pressed() {
  oled_screen_display_bitmap(epd_bitmap_toggle_btn_pressed, 0, 0, 128, 64,
                             OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_toggle_released() {
  oled_screen_display_bitmap(epd_bitmap_toggle_btn_released, 0, 0, 128, 64,
                             OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_creating_network() {
  oled_screen_clear();
  oled_screen_display_text("Creating", 25, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("network...", 20, 4, OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_creating_network_failed() {
  oled_screen_clear();
  oled_screen_display_text("Creating", 25, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("network", 29, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("failed", 33, 5, OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_waiting_for_devices(uint8_t dots) {
  dots = dots > 3 ? 0 : dots;
  oled_screen_clear_line(80, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Waiting for", 19, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("devices", 24, 4, OLED_DISPLAY_NORMAL);
  // Print dots from lef to right
  for (int i = 0; i < dots; i++) {
    oled_screen_display_text(".", 80 + (i * 8), 4, OLED_DISPLAY_NORMAL);
  }
}

void zigbee_screens_module_no_devices_found() {
  oled_screen_clear();
  oled_screen_display_text("No devices", 24, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("found", 44, 4, OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_closing_network() {
  oled_screen_clear();
  oled_screen_display_text("Closing", 25, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("network...", 20, 4, OLED_DISPLAY_NORMAL);
}

///////////////////////////////////////////////////////////////////////////

void zigbee_screens_display_scanning_animation() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("ZIGBEE SNIFFER", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Channel: ", 6, OLED_DISPLAY_INVERT);
  while (true) {
    for (int i = 0; i < zigbee_bitmap_allArray_LEN; i++) {
      oled_screen_display_bitmap(zigbee_bitmap_allArray[i], 0, 16, 128, 32,
                                 OLED_DISPLAY_NORMAL);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void zigbee_screens_display_scanning_text(int count, int channel) {
  oled_screen_clear_line(0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 7, OLED_DISPLAY_NORMAL);
  char* packets_count = (char*) malloc(17);
  char* channel_text = (char*) malloc(17);
  sprintf(packets_count, "Packets: %d", count);
  sprintf(channel_text, "Channel: %d", channel);
  oled_screen_display_text_center(channel_text, 6, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center(packets_count, 7, OLED_DISPLAY_INVERT);
  free(packets_count);
  free(channel_text);
}
