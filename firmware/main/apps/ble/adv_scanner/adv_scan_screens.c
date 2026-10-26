#include "adv_scan_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general_screens.h"
#include "led_events.h"
#include "oled_screen.h"

#define MAX_ITEMS_PER_SCREEN 5

static esp_ble_gap_cb_param_t adv_list[MAX_ITEMS_PER_SCREEN];
static uint8_t adv_list_count = 0;
static uint8_t s_records_filled = 0;

void adv_scanner_clear_records(void) {
  memset(adv_list, 0, sizeof(adv_list));
  adv_list_count = 0;
  s_records_filled = 0;
}

void adv_scanner_display_record(esp_ble_gap_cb_param_t* record) {
  if (record == NULL) {
    return;
  }
  if (adv_list_count >= MAX_ITEMS_PER_SCREEN) {
    adv_list_count = 0;
  }
  adv_list[adv_list_count] = *record;
  adv_list_count++;
  if (s_records_filled < MAX_ITEMS_PER_SCREEN) {
    s_records_filled++;
  }

  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("ADV Type  | RSSI", 0, 1, OLED_DISPLAY_NORMAL);

  char row[45];
  for (int i = 0; i < s_records_filled; i++) {
    uint8_t t = adv_list[i].scan_rst.ble_evt_type;
    const char* type_str = (t < 5) ? evt_adv_type[t] : "EXT_ADV";
    snprintf(row, sizeof(row), "%-10s| %d", type_str,
             adv_list[i].scan_rst.rssi);
    oled_screen_display_text(row, 0, i + 2, OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}