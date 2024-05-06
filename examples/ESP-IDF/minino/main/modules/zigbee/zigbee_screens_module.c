#include "zigbee_screens_module.h"
#include "zigbee_bitmaps.h"

void zigbee_screens_module_toogle_pressed() {
  display_bitmap(epd_bitmap_toggle_btn_pressed, 0, 0, 128, 64, NO_INVERT);
}

void zigbee_screens_module_toggle_released() {
  display_bitmap(epd_bitmap_toggle_btn_released, 0, 0, 128, 64, NO_INVERT);
}

void zigbee_screens_module_creating_network() {
  display_clear();
  display_text("Creating", 25, 3, NO_INVERT);
  display_text("network...", 20, 4, NO_INVERT);
}

void zigbee_screens_module_creating_network_failed() {
  display_clear();
  display_text("Creating", 25, 3, NO_INVERT);
  display_text("network", 29, 4, NO_INVERT);
  display_text("failed", 33, 5, NO_INVERT);
}

void zigbee_screens_module_waiting_for_devices(uint8_t dots) {
  dots = dots > 3 ? 0 : dots;
  display_clear_line(80, 4, NO_INVERT);
  display_text("Waiting for", 19, 3, NO_INVERT);
  display_text("devices", 24, 4, NO_INVERT);
  // Print dots from lef to right
  for (int i = 0; i < dots; i++) {
    display_text(".", 80 + (i * 8), 4, NO_INVERT);
  }
}

void zigbee_screens_module_closing_network() {
  display_clear();
  display_text("Closing", 25, 3, NO_INVERT);
  display_text("network...", 20, 4, NO_INVERT);
}
