#include "apps/ble/hid_device/hid_module.h"
#include "apps/ble/hid_device/hid_screens.h"
#include "ble_hidd_main.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "general_submenu.h"
#include "menus_module.h"

static uint16_t current_item = 0;

static void hid_module_cb_event(uint8_t button_name, uint8_t button_event);
static void hid_module_cb_event_volumen(uint8_t button_name,
                                        uint8_t button_event);
static void hid_module_cb_connection_handler(bool connection);

static void hid_module_reset_menu() {
  current_item = 0;
  hid_module_register_menu(GENERAL_TREE_APP_MENU);
  hid_module_display_menu(current_item);
  menus_module_set_app_state(true, hid_module_cb_event);
}

static void hid_module_reset_start_menu() {
  // ble_scanner_register_cb(NULL);
  menus_module_reset();
  hid_module_reset_menu();
}

void hid_module_display_filter() {
  general_submenu_menu_t hid_menu_filter = {0};
  hid_menu_filter.options = hid_device_items;
  hid_menu_filter.options_count = 4;
  hid_menu_filter.exit_cb = hid_module_reset_menu;
  general_submenu(hid_menu_filter);
}

static void hid_module_cb_connection_handler(bool connection) {
  hid_module_display_device_connection(connection);
  if (!connection) {
    // esp_restart();
  }
}

static void hid_module_cb_event_volumen(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      current_item = current_item-- == 0 ? HID_DEVICE_COUNT - 1 : current_item;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > HID_DEVICE_COUNT - 1 ? 0 : current_item;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_LEFT:
      current_item = 0;
      hid_module_register_menu(GENERAL_TREE_APP_MENU);
      hid_module_display_menu(current_item);
      menus_module_set_app_state(true, hid_module_cb_event);
      break;
    case BUTTON_RIGHT:
      if (current_item == HID_DEVICE_VOL_UP) {
        ble_hid_volume_up(true);
        ble_hid_volume_up(false);
        hid_module_display_notify_volumen_up();
      } else if (current_item == HID_DEVICE_VOL_DOWN) {
        ble_hid_volume_down(true);
        ble_hid_volume_down(false);
        hid_module_display_notify_volumen_down();
      } else if (current_item == HID_DEVICE_PLAY) {
        ble_hid_play(true);
        ble_hid_play(false);
        hid_module_display_notify_play_pause();
      } else if (current_item == HID_DEVICE_MUTE) {
        ble_hid_mute(true);
        ble_hid_mute(false);
        hid_module_display_notify_mute();
      } else if (current_item == HID_DEVICE_NEXT_TRACK) {
        ble_hid_next_track(true);
        ble_hid_next_track(false);
        hid_module_display_notify_next_track();
      } else if (current_item == HID_DEVICE_STOP) {
        ble_hid_stop(true);
        ble_hid_stop(false);
        hid_module_display_notify_stop();
      } else if (current_item == HID_DEVICE_PAUSE) {
        ble_hid_pause(true);
        ble_hid_pause(false);
        hid_module_display_notify_pause();
      } else if (current_item == HID_DEVICE_PREV_TRACK) {
        ble_hid_prev_track(true);
        ble_hid_prev_track(false);
        hid_module_display_notify_prev_track();
      }
      break;
    default:
      break;
  }
}

static void hid_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      current_item = current_item-- == 0 ? HID_MENU_COUNT - 1 : current_item;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = ++current_item > HID_MENU_COUNT - 1 ? 0 : current_item;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      if (current_item == HID_CONFIG_NAME) {
        current_item = 0;
        char* hid_name[20];
        ble_hid_get_device_name(&hid_name);
        general_screen_display_card_information_handler(
            "Device Name", &hid_name, hid_module_reset_menu,
            hid_module_cb_event);
      } else if (current_item == HID_CONFIG_MAC) {
        current_item = 0;
        uint8_t hid_mac[8] = {0};
        esp_read_mac(hid_mac, ESP_MAC_BT);
        char mac_address[20];
        sprintf(mac_address, "%02X:%02X:%02X:%02X", hid_mac[2], hid_mac[3],
                hid_mac[4], hid_mac[5]);
        general_screen_display_card_information_handler(
            "Device MAC", &mac_address, hid_module_reset_menu,
            hid_module_cb_event);
      } else if (current_item == HID_CONFIG_START) {
        current_item = 0;
        // hid_module_register_menu(GENERAL_TREE_APP_SUBMENU);

        general_screen_display_card_information_handler(
            "HID", "HID control", hid_module_reset_start_menu,
            hid_scanner_module_reset_start_menu);

        hid_module_display_device_pairing();
        ble_hid_register_callback(hid_module_cb_connection_handler);
        ble_hid_begin();
        menus_module_set_app_state(true, hid_module_cb_event_volumen);
      }
      break;
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    default:
      break;
  }
}

void hid_module_begin() {
  hid_module_register_menu(GENERAL_TREE_APP_MENU);
  hid_module_display_menu(current_item);
  menus_module_set_app_state(true, hid_module_cb_event);
}
