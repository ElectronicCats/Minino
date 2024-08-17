#include "spam_screens.h"
#include <string.h>
#include "animations_task.h"
#include "oled_screen.h"

static void ble_screens_display_scanning_animation() {
  static uint8_t frame = 0;
  oled_screen_display_bitmap(ble_bitmap_scan_attack_allArray[frame], 0, 16, 128,
                             32, OLED_DISPLAY_NORMAL);
  frame = ++frame > 3 ? 0 : frame;
}

void ble_screens_start_scanning_animation() {
  oled_screen_clear();
  oled_screen_display_text_center("BLE SPAM", 0, OLED_DISPLAY_NORMAL);
  animations_task_run(ble_screens_display_scanning_animation, 100, NULL);
}

void ble_screens_display_scanning_text(char* name) {
  oled_screen_clear_line(0, 7, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(name, 7, OLED_DISPLAY_INVERT);
}
