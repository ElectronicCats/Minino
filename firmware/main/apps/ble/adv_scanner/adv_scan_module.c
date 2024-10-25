#include "adv_scan_screens.h"
#include "ble_scann.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "general_submenu.h"
#include "menus_module.h"

static uint16_t current_item = 0;

static void adv_scanner_module_cb_event(uint8_t button_name,
                                        uint8_t button_event);

static void adv_scanner_module_reset_menu() {
  current_item = 0;
  adv_scanner_module_register_menu(GENERAL_TREE_APP_MENU);
  adv_scanner_module_display_menu(current_item);
  menus_module_set_app_state(true, adv_scanner_module_cb_event);
}

static void adv_filter_selection(uint8_t selection) {
  set_filter_type(selection);
}

void adv_scanner_display_filter() {
  general_submenu_menu_t adv_menu_filter;
  adv_menu_filter.options = scan_filter_items;
  adv_menu_filter.options_count = 4;
  adv_menu_filter.select_cb = adv_filter_selection;
  adv_menu_filter.exit_cb = adv_scanner_module_reset_menu;
  general_submenu(adv_menu_filter);
}

static void adv_scanner_module_cb_event(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      current_item = current_item-- == 0 ? SCAN_MENU_COUNT - 1 : current_item;
      adv_scanner_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > SCAN_MENU_COUNT - 1 ? 0 : current_item;
      adv_scanner_module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      if (current_item == SCAN_TYPE) {
        adv_scanner_display_filter();
      } else if (current_item == SCAN_START) {
        ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Scan start");
        ble_scanner_register_cb(adv_scanner_display_record);
        ble_scanner_begin();
      }
      current_item = 0;

      break;
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    default:
      break;
  }
}

void adv_scanner_module_begin() {
  adv_scanner_module_register_menu(GENERAL_TREE_APP_MENU);
  adv_scanner_module_display_menu(current_item);
  menus_module_set_app_state(true, adv_scanner_module_cb_event);
}
