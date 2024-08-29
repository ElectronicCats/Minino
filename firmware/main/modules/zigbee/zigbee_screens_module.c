#include "zigbee_screens_module.h"
#include "general/general_screens.h"
#include "oled_screen.h"
#include "zigbee_bitmaps.h"
#include "zigbee_switch.h"

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
  genera_screen_display_card_information("Creating network", "Please wait...");
}

void zigbee_screens_module_creating_network_failed() {
  oled_screen_clear();
  genera_screen_display_card_information("Creating network", "Failed");
}

void zigbee_screens_module_waiting_for_devices() {
  static uint8_t dots = 0;
  dots = ++dots > 3 ? 0 : dots;
  genera_screen_display_card_information("Waiting for", "devices");
  // Print dots from lef to right
  for (int i = 0; i < 3; i++) {
    oled_screen_display_text(i < dots ? "." : "", 56 + (i * 8), 4,
                             OLED_DISPLAY_NORMAL);
  }
}

void zigbee_screens_module_no_devices_found() {
  oled_screen_clear();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  genera_screen_display_card_information("No devices", "found");
}

void zigbee_screens_module_closing_network() {
  oled_screen_clear();
  genera_screen_display_card_information("Closing network", "Please wait...");
}

void zigbee_screens_module_display_status(uint8_t status) {
  switch (status) {
    case CREATING_NETWORK:
      zigbee_screens_module_creating_network();
      break;
    case CREATING_NETWORK_FAILED:
      zigbee_screens_module_creating_network_failed();
      break;
    case WAITING_FOR_DEVICES:
      zigbee_screens_module_waiting_for_devices();
      break;
    case NO_DEVICES_FOUND:
      zigbee_screens_module_no_devices_found();
      break;
    case CLOSING_NETWORK:
      zigbee_screens_module_closing_network();
      break;
    case LIGHT_PRESSED:
      zigbee_screens_module_toogle_pressed();
      break;
    case LIGHT_RELASED:
      zigbee_screens_module_toggle_released();
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////
void zigbee_screens_display_device_ad() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  int index_page = 0;
  oled_screen_display_text_splited("Use our", &index_page, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("PyCatSniffer", &index_page,
                                   OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("to communicate with", &index_page,
                                   OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("Wireshark", &index_page,
                                   OLED_DISPLAY_NORMAL);
}

void zigbee_screens_display_zigbee_sniffer_text() {
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("ZIGBEE SNIFFER", 0, OLED_DISPLAY_NORMAL);
}

void zigbee_screens_display_scanning_animation() {
  static uint8_t frame = 0;
  oled_screen_display_bitmap(zigbee_bitmap_allArray[frame], 0, 16, 128, 32,
                             OLED_DISPLAY_NORMAL);
  frame = ++frame > zigbee_bitmap_allArray_LEN - 1 ? 0 : frame;
}

void zigbee_screens_display_scanning_text(int count) {
  oled_screen_clear_line(0, 6, OLED_DISPLAY_NORMAL);
  char* packets_count = (char*) malloc(17);
  sprintf(packets_count, "Packets: %d", count);
  oled_screen_display_text_center(packets_count, 6, OLED_DISPLAY_INVERT);
  free(packets_count);
}
