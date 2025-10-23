#include "adv_scan_screens.h"
#include "ble_scann.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "general_submenu.h"
#include "menus_module.h"

uint16_t last_selection = 0;

static void adv_scanner_display_filter();
static void adv_scanner_display_type();

static void adv_scanner_menu_handler_selector(uint8_t option) {
  last_selection = option;
  switch (option) {
    case SCAN_TYPE:
      adv_scanner_display_type();
      break;
    case SCAN_FILTER:
      adv_scanner_display_filter();
      break;
    case SCAN_START:
      ble_scanner_register_cb(adv_scanner_display_record);
      ble_scanner_begin();
      break;
    default:
      break;
  }
}

static void adv_scanner_display_main() {
  general_submenu_menu_t adv_menu_main = {0};
  adv_menu_main.options = (const char**) scan_menu_items;
  adv_menu_main.options_count = SCAN_MENU_COUNT;
  adv_menu_main.select_cb = adv_scanner_menu_handler_selector;
  adv_menu_main.exit_cb = menus_module_reset;
  adv_menu_main.selected_option = last_selection;
  general_submenu(adv_menu_main);
}

static void adv_filter_selection(uint8_t selection) {
  set_filter_type(selection);
  adv_scanner_display_main();
}

static void adv_type_selection(uint8_t selection) {
  set_scan_type(selection);
  adv_scanner_display_main();
}

static void adv_scanner_display_filter() {
  general_submenu_menu_t adv_menu_filter = {0};
  adv_menu_filter.options = (const char**) scan_filter_items;
  adv_menu_filter.options_count = 4;
  adv_menu_filter.select_cb = adv_filter_selection;
  adv_menu_filter.exit_cb = adv_scanner_display_main;
  general_submenu(adv_menu_filter);
}

static void adv_scanner_display_type() {
  general_submenu_menu_t adv_menu_type = {0};
  adv_menu_type.options = (const char**) scan_type_items;
  adv_menu_type.options_count = 2;
  adv_menu_type.select_cb = adv_type_selection;
  adv_menu_type.exit_cb = adv_scanner_display_main;
  general_submenu(adv_menu_type);
}

void adv_scanner_module_begin() {
  adv_scanner_display_main();
}
