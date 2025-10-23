#include "adv_scan_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "oled_screen.h"

#define MAX_ITEMS_PER_SCREEN 5

static esp_ble_gap_cb_param_t adv_list[MAX_ITEMS_PER_SCREEN];
static uint8_t adv_list_count = 0;

void adv_scanner_display_record(esp_ble_gap_cb_param_t* record) {
  if (adv_list_count >= MAX_ITEMS_PER_SCREEN) {
    adv_list_count = 0;
  }
  adv_list[adv_list_count] = *record;

  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("ADV Type  | RSSI", 0, 1, OLED_DISPLAY_NORMAL);

  char row[45];
  for (int i = 0; i < MAX_ITEMS_PER_SCREEN; i++) {
    sprintf(row, "%s %d", evt_adv_type[adv_list[i].scan_rst.ble_evt_type],
            adv_list[i].scan_rst.rssi);
    oled_screen_display_text(row, 0, i + 2, OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
  adv_list_count++;
}