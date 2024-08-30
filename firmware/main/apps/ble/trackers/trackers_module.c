#include "apps/ble/trackers/trackers_module.h"
#include "apps/ble/trackers/trackers_screens.h"
#include "esp_log.h"
#include "menus_module.h"
#include "trackers_scanner.h"

static uint16_t current_item = 0;
static uint16_t trackers_count = 0;
static bool trackers_scanned = false;
static tracker_profile_t* scanned_airtags = NULL;
char* tracker_information[4] = {
    NULL,
    NULL,
    "MAC ADDRS",
    NULL,
};

static const general_menu_t trackers_information = {
    .menu_items = tracker_information,
    .menu_count = 4,
    .menu_level = GENERAL_TREE_APP_INFORMATION,
};

static void module_main_cb_event(uint8_t button_name, uint8_t button_event);
static void module_list_cb_event(uint8_t button_name, uint8_t button_event);

static void module_reset_menu() {
  current_item = 0;
  module_register_menu(GENERAL_TREE_APP_SUBMENU);
  module_display_menu(current_item);
  menus_module_set_app_state(true, module_list_cb_event);
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
      current_item = current_item-- == 0 ? TRACKERS_COUNT - 1 : current_item;
      module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > TRACKERS_COUNT - 1 ? 0 : current_item;
      module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
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
        menus_module_set_app_state(true, module_main_cb_event);
      } else {
        menus_module_restart();
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
      current_item = current_item-- == 0 ? TRACKERS_COUNT - 1 : current_item;
      module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > TRACKERS_COUNT - 1 ? 0 : current_item;
      module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      char tracker_mac[18];
      snprintf(tracker_mac, sizeof(tracker_mac), "%02X:%02X:%02X:%02X:%02X",
               scanned_airtags[current_item].mac_address[1],
               scanned_airtags[current_item].mac_address[2],
               scanned_airtags[current_item].mac_address[3],
               scanned_airtags[current_item].mac_address[4],
               scanned_airtags[current_item].mac_address[5]);
      char tracker_rssi[18];
      snprintf(tracker_rssi, sizeof(tracker_rssi), "RSSI: %d",
               scanned_airtags[current_item].rssi);
      char tracker_name[18];
      snprintf(tracker_name, sizeof(tracker_name), "Name: %s",
               scanned_airtags[current_item].name);
      tracker_information[0] = malloc(strlen(tracker_name) + 1);
      strcpy(tracker_information[0], tracker_name);
      tracker_information[1] = malloc(strlen(tracker_rssi) + 1);
      strcpy(tracker_information[1], tracker_rssi);
      tracker_information[3] = malloc(strlen(tracker_mac) + 1);
      strcpy(tracker_information[3], tracker_mac);
      general_register_scrolling_menu(&trackers_information);
      general_screen_display_scrolling_text_handler(module_reset_menu);
      break;
    case BUTTON_LEFT:
      if (tracker_information[0] != NULL) {
        free(tracker_information[0]);
        free(tracker_information[1]);
        free(tracker_information[3]);
        tracker_information[0] = NULL;
        tracker_information[1] = NULL;
        tracker_information[3] = NULL;
      }
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
