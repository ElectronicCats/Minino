#include "ota_module_screens.h"
#include <string.h>
#include "oled_screen.h"

#define BAR_HEIGHT 8
#define BAR_WIDTH  64

uint8_t bar_bitmap[BAR_HEIGHT][BAR_WIDTH / 8];

void ota_module_screens_show_help() {
  oled_screen_display_text("**-Connect to-**", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("SSID: minino_ap ", 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Pass: Cats1234  ", 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Then open       ", 0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("192.168.0.1     ", 0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("in your browser.", 0, 7, OLED_DISPLAY_NORMAL);
}

static void update_bar(uint8_t value) {
  uint8_t active_cols = (uint32_t) value * BAR_WIDTH / 100;
  memset(bar_bitmap, 0, sizeof(bar_bitmap));
  for (int y = 0; y < BAR_HEIGHT; y++) {
    for (int x = 0; x < active_cols; x++) {
      bar_bitmap[y][x / 8] |= (1 << (7 - (x % 8)));
    }
  }
}
static void show_update_status(uint8_t* progress) {
  oled_screen_display_text(" UPLOADING FILE ", 0, 0, OLED_DISPLAY_INVERT);
  oled_screen_clear_line(0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Please dont turn", 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("off the device  ", 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Now: v1.1.0.0", 0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("New: v1.1.0.1", 0, 6, OLED_DISPLAY_NORMAL);
  update_bar(*progress);
  oled_screen_display_bitmap(bar_bitmap, 0, 120, BAR_WIDTH, BAR_HEIGHT,
                             OLED_DISPLAY_NORMAL);
}

void ota_module_screens_show_event(ota_show_events_t event, void* context) {
  switch (event) {
    case OTA_SHOW_PROGRESS_EVENT:
      show_update_status(context);
      break;
    default:
      break;
  }
}