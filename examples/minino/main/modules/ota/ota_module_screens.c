#include "ota_module_screens.h"
#include <string.h>
#include "oled_screen.h"

#define BAR_HEIGHT 8
#define BAR_WIDTH  128

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
static void show_start_status() {
  oled_screen_display_text(" UPLOADING FILE ", 0, 0, OLED_DISPLAY_INVERT);
  oled_screen_clear_line(0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Please dont turn", 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("off the device  ", 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Now: " CONFIG_PROJECT_VERSION, 0, 5,
                           OLED_DISPLAY_NORMAL);
  // TODO: Change to the new version
  oled_screen_display_text("New: vx.x.x.x   ", 0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_clear_line(0, 7, OLED_DISPLAY_NORMAL);
}

static void show_update_status(uint8_t* progress) {
  char* str = (char*) malloc(20);
  sprintf(str, "%d%%", *progress);
  oled_screen_display_text_center(str, 7, OLED_DISPLAY_INVERT);
  free(str);
}

static void show_result_status(bool* flash_successful) {
  oled_screen_clear();
  if (*flash_successful) {
    printf("OTA SUCCESS\n");
    oled_screen_display_text("   SUCCESS!!!   ", 0, 3, OLED_DISPLAY_INVERT);
    oled_screen_display_text("Rebooting System", 0, 5, OLED_DISPLAY_NORMAL);
  } else {
    printf("OTA FAIL\n");
    oled_screen_display_text("    ERROR!!!    ", 0, 1, OLED_DISPLAY_INVERT);
    oled_screen_display_text(" Something was  ", 0, 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_text("     wrong      ", 0, 4, OLED_DISPLAY_NORMAL);
    oled_screen_display_text("   Try again    ", 0, 6, OLED_DISPLAY_NORMAL);
  }
}
void ota_module_screens_show_event(ota_show_events_t event, void* context) {
  switch (event) {
    case OTA_SHOW_START_EVENT:
      show_start_status();
      break;
    case OTA_SHOW_PROGRESS_EVENT:
      show_update_status(context);
      break;
    case OTA_SHOW_RESULT_EVENT:
      show_result_status(context);
    default:
      break;
  }
}