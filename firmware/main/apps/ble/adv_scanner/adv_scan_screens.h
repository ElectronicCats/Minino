#include <stdbool.h>
#include <stdint.h>
#include "esp_gap_ble_api.h"
#include "general_screens.h"
#ifndef ADV_SCAN_SCREENS_H
  #define ADV_SCAN_SCREENS_H

enum { SCAN_TYPE, SCAN_FILTER, SCAN_START, SCAN_MENU_COUNT };
extern char* scan_menu_items[];

extern char* scan_type_items[];
extern char* scan_filter_items[];
extern char* evt_adv_type[];

void adv_scanner_module_register_menu(menu_tree_t menu);
void adv_scanner_clear_screen();
void adv_scanner_module_display_menu(uint16_t current_item);
void adv_scanner_display_record(esp_ble_gap_cb_param_t* record);
void adv_scanner_clear_records(void);
#endif  // ADV_SCAN_SCREENS_H
