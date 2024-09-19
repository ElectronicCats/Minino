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
  oled_screen_display_text_center("< Back", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("BLE SPAM", 1, OLED_DISPLAY_NORMAL);
#ifdef CONFIG_RESOLUTION_128X64
  animations_task_run(ble_screens_display_scanning_animation, 100, NULL);
#endif
}

void ble_screens_display_scanning_text(char* name) {
  int page = 7;
#ifdef CONFIG_RESOLUTION_128X32
  page = 2;
#endif
  oled_screen_clear_line(0, page, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(name, page, OLED_DISPLAY_INVERT);
}
