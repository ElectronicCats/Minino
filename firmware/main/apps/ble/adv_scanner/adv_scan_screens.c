#include "adv_scan_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "oled_screen.h"

#define MAX_ITEMS_PER_SCREEN 5

static uint16_t hid_current_item = 0;
static esp_ble_gap_cb_param_t adv_list[MAX_ITEMS_PER_SCREEN];
static uint8_t adv_list_count = 0;

static const general_menu_t adv_menu_main = {
    .menu_items = scan_menu_items,
    .menu_count = SCAN_MENU_COUNT,
    .menu_level = GENERAL_TREE_APP_MENU,
};

// static const general_menu_t adv_type = {
//     .menu_items = scan_type_items,
//     .menu_count = 2,
//     .menu_level = GENERAL_TREE_APP_MENU,
// };

// static const general_menu_t adv_filter = {
//     .menu_items = scan_filter_items,
//     .menu_count = 4,
//     .menu_level = GENERAL_TREE_APP_MENU,
// };

void adv_scanner_module_register_menu(menu_tree_t menu) {
  switch (menu) {
    case GENERAL_TREE_APP_MENU:
      general_register_menu(&adv_menu_main);
      break;
    default:
      general_register_menu(&adv_menu_main);
      break;
  }
}

void adv_scanner_display_record(esp_ble_gap_cb_param_t* record) {
  if (adv_list_count >= MAX_ITEMS_PER_SCREEN) {
    adv_list_count = 0;
  }
  adv_list[adv_list_count] = *record;

  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("ADV Type  | RSSI", 0, 1, OLED_DISPLAY_NORMAL);

  char* row = (char*) malloc(17);
  for (int i = 0; i < MAX_ITEMS_PER_SCREEN; i++) {
    sprintf(row, "%s %d", evt_adv_type[adv_list[i].scan_rst.ble_evt_type],
            adv_list[i].scan_rst.rssi);
    oled_screen_display_text(row, 0, i + 2, OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
  adv_list_count++;
}

void adv_scanner_module_display_menu(uint16_t current_item) {
  led_control_run_effect(led_control_pulse_leds);
  hid_current_item = current_item;
  general_screen_display_menu(current_item);
}