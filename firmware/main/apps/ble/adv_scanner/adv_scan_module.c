#include "adv_scan_screens.h"
#include "ble_scann.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"

static uint16_t last_selection = 0;

static void adv_scanner_display_main();
static void adv_scanner_display_filter();
static void adv_scanner_display_type();

static void adv_scanner_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN &&
      button_event != BUTTON_LONG_PRESS_HOLD) {
    return;
  }
  if (button_name == BUTTON_LEFT) {
    ble_scanner_stop();
    ble_scanner_register_cb(NULL);
    oled_screen_clear();
    menus_module_set_default_input();
    adv_scanner_display_main();
  }
}

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
      adv_scanner_clear_records();
      menus_module_set_app_state(true, adv_scanner_input_cb);
      oled_screen_clear_buffer();
      oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
      oled_screen_display_text("Scanning...", 0, 2, OLED_DISPLAY_NORMAL);
      oled_screen_display_show();
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
  adv_menu_main.exit_cb = menus_module_exit_app;
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

void adv_scanner_module_stop(void) {
  ble_scanner_stop();
  ble_scanner_register_cb(NULL);
}
