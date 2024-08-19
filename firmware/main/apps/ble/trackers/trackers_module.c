#include "apps/ble/trackers/trackers_module.h"
#include "apps/ble/trackers/trackers_screens.h"
#include "esp_log.h"
#include "menus_module.h"
#include "trackers_scanner.h"

static uint16_t current_item = 0;
static uint16_t trackers_count = 0;
static bool trackers_scanned = false;
static tracker_profile_t* scanned_airtags = NULL;

static void module_main_cb_event(uint8_t button_name, uint8_t button_event);
static void module_list_cb_event(uint8_t button_name, uint8_t button_event);

static void module_increment_item() {
  current_item++;
}

static void module_decrement_item() {
  current_item--;
}

static void module_handle_trackers(tracker_profile_t record) {
  int device_exists = trackers_scanner_find_profile_by_mac(
      scanned_airtags, trackers_count, record.mac_address);
  if (device_exists == -1) {
    if (scanned_airtags == NULL) {
      module_register_menu(GENERAL_TREE_APP_MENU);
    }
    module_add_tracker_to_list(record.name);
    module_display_device_detected(record.name);
    trackers_scanner_add_tracker_profile(&scanned_airtags, &trackers_count,
                                         record);
  } else {
    scanned_airtags[device_exists].rssi = record.rssi;
    module_update_tracker_name(scanned_airtags[device_exists].name,
                               device_exists);
  }
}

static void module_main_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      module_decrement_item();
      if (current_item < 0) {
        current_item = TRACKERS_COUNT - 1;
      }
      module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      module_increment_item();
      if (current_item > TRACKERS_COUNT - 1) {
        current_item = 0;
      }
      module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      // if(trackers_scanned){
      //   trackers_scanned = false;
      //   module_update_scan_state(trackers_scanned);
      //   trackers_scanner_stop();
      //   module_display_menu(current_item);
      //   break;
      // }
      if (current_item == TRACKERS_SCAN && !trackers_scanned) {
        trackers_scanned = true;
        module_update_scan_state(trackers_scanned);
        module_display_scanning();
        trackers_scanner_register_cb(module_handle_trackers);
        trackers_scanner_start();
      } else if (current_item == TRACKERS_LIST) {
        current_item = 0;
        module_register_menu(GENERAL_TREE_APP_SUBMENU);
        module_display_menu(current_item);
        menus_module_set_app_state(true, module_list_cb_event);
      }
      break;
    case BUTTON_LEFT:
      if (current_item == TRACKERS_LIST) {
        module_register_menu(GENERAL_TREE_APP_MENU);
        module_display_menu(current_item);
      } else {
        menus_module_exit_app();
      }
      break;
    default:
      break;
  }
}

static void module_list_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      module_decrement_item();
      if (current_item < 0) {
        current_item = TRACKERS_COUNT - 1;
      }
      module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      module_increment_item();
      if (current_item > TRACKERS_COUNT - 1) {
        current_item = 0;
      }
      module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      char tracker_information[45];
      snprintf(tracker_information, sizeof(tracker_information),
               "Tracker: %s RSSI: %d dBm MAC: %02X:%02X:%02X",
               scanned_airtags[current_item].name,
               scanned_airtags[current_item].rssi,
               scanned_airtags[current_item].mac_address[3],
               scanned_airtags[current_item].mac_address[4],
               scanned_airtags[current_item].mac_address[5]);
      module_display_tracker_information("Information", tracker_information);
      break;
    case BUTTON_LEFT:
      menus_module_set_app_state(true, module_main_cb_event);
      module_register_menu(GENERAL_TREE_APP_MENU);
      module_display_menu(current_item);
      break;
    default:
      break;
  }
}

void trackers_module_begin() {
  module_register_menu(GENERAL_TREE_APP_MENU);
  module_display_menu(current_item);
  menus_module_set_app_state(true, module_main_cb_event);
}
