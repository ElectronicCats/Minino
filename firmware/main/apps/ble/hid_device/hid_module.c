#include "apps/ble/hid_device/hid_module.h"
#include "apps/ble/hid_device/hid_screens.h"
#include "ble_hidd_main.h"
#include "esp_log.h"
#include "menu_screens_modules.h"

static uint16_t current_item = 0;

static void hid_module_cb_event(uint8_t button_name, uint8_t button_event);
static void hid_module_cb_event_volumen(uint8_t button_name,
                                        uint8_t button_event);
static void hid_module_cb_connection_handler(bool connection);

static void hid_module_increment_item() {
  current_item++;
}

static void hid_module_decrement_item() {
  current_item--;
}

static void hid_module_cb_connection_handler(bool connection) {
  hid_module_display_device_connection(connection);
  if (!connection) {
    esp_restart();
  }
}

static void hid_module_cb_event_volumen(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      hid_module_decrement_item();
      if (current_item < 0) {
        current_item = HID_DEVICE_COUNT - 1;
      }
      hid_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      hid_module_increment_item();
      if (current_item > HID_DEVICE_COUNT - 1) {
        current_item = 0;
      }
      hid_module_display_menu(current_item);
      break;
    case BUTTON_LEFT:
      current_item = 0;
      hid_module_register_menu(HID_TREE_MENU);
      hid_module_display_menu(current_item);
      menu_screens_set_app_state(true, hid_module_cb_event);
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
      hid_module_decrement_item();
      if (current_item < 0) {
        current_item = HID_MENU_COUNT - 1;
      }
      hid_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      hid_module_increment_item();
      if (current_item > HID_MENU_COUNT - 1) {
        current_item = 0;
      }
      hid_module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      if (current_item == HID_CONFIG_NAME) {
        current_item = 0;
        hid_module_display_device_name();
      } else if (current_item == HID_CONFIG_MAC) {
        current_item = 0;
        hid_module_display_device_mac();
      } else if (current_item == HID_CONFIG_START) {
        current_item = 0;
        hid_module_register_menu(HID_TREE_DEVICE);
        hid_module_display_device_pairing();
        ble_hid_register_callback(hid_module_cb_connection_handler);
        ble_hid_begin();
        menu_screens_set_app_state(true, hid_module_cb_event_volumen);
      }
      break;
    case BUTTON_LEFT:
      current_item = 0;
      hid_module_register_menu(HID_TREE_MENU);
      hid_module_display_menu(current_item);
      break;
    default:
      break;
  }
}

void hid_module_begin() {
  hid_module_register_menu(HID_TREE_MENU);
  hid_module_display_menu(current_item);
  menu_screens_set_app_state(true, hid_module_cb_event);
}
