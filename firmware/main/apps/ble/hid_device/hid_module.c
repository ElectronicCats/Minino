#include "apps/ble/hid_device/hid_module.h"
#include "apps/ble/hid_device/hid_screens.h"
#include "ble_hidd_main.h"
#include "esp_log.h"
#include "esp_mac.h"
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
        hid_module_register_menu(GENERAL_TREE_APP_SUBMENU);
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
