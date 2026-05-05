#include <stdbool.h>
#include <stdint.h>
#include "esp_gap_ble_api.h"
#include "general_screens.h"
#ifndef ADV_SCAN_SCREENS_H
  #define ADV_SCAN_SCREENS_H

enum {
  SCAN_TYPE,
  SCAN_FILTER,
  SCAN_START,
  SCAN_MENU_COUNT
} scan_menu_item = SCAN_FILTER;
char* scan_menu_items[SCAN_MENU_COUNT] = {"Scan Type", "ADV Filter", "Start"};

char* scan_type_items[2] = {"Active", "Passive"};
char* scan_filter_items[4] = {"Allow All", "Allow Only WLST", "Allow UND RPA",
                              "Allow WLST & RPA"};
char* evt_adv_type[5] = {"IND", "DIRECT_IND", "SCAN_IND", "NONCONN_IND",
                         "SCAN_RSP"};

void adv_scanner_module_register_menu(menu_tree_t menu);
void adv_scanner_clear_screen();
void adv_scanner_module_display_menu(uint16_t current_item);
void adv_scanner_display_record(esp_ble_gap_cb_param_t* record);
#endif  // ADV_SCAN_SCREENS_H
