#include "zigbee_screens.h"
#include "zigbee_bitmaps.h"

void display_zb_switch_toggle_pressed() {
  display_bitmap(epd_bitmap_toggle_btn_pressed, 0, 0, 128, 64, NO_INVERT);
}

void display_zb_switch_toggle_released() {
  display_bitmap(epd_bitmap_toggle_btn_released, 0, 0, 128, 64, NO_INVERT);
}
