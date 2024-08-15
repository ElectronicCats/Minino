#include "hid_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general/bitmaps_general.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "oled_screen.h"

static uint16_t hid_current_item = 0;

static const general_menu_t hid_menu = {
    .menu_items = hid_menu_items,
    .menu_count = HID_MENU_COUNT,
    .menu_level = GENERAL_MENU_MAIN,
};

static const general_menu_t hid_device_menu = {
    .menu_items = hid_device_items,
    .menu_count = HID_DEVICE_COUNT,
    .menu_level = GENERAL_MENU_SUBMENU,
};

void hid_module_register_menu(hid_menu_t menu) {
  switch (menu) {
    case HID_TREE_MENU:
      general_register_menu(&hid_menu);
      break;
    case HID_TREE_DEVICE:
      general_register_menu(&hid_device_menu);
      break;
    default:
      general_register_menu(&hid_menu);
      break;
  }
}

void hid_module_display_notify_volumen_up() {
  led_control_run_effect(led_control_pulse_led_right);
  genera_screen_display_notify_information("Notify", "Volumen Up");
  vTaskDelay(500 / portTICK_PERIOD_MS);
  general_screen_display_menu(hid_current_item);
}

void hid_module_display_notify_volumen_down() {
  led_control_run_effect(led_control_pulse_led_left);
  genera_screen_display_notify_information("Notify", "Volumen Down");
  vTaskDelay(500 / portTICK_PERIOD_MS);
  general_screen_display_menu(hid_current_item);
}

void hid_module_display_notify_play_pause() {
  led_control_run_effect(led_control_ble_tracking);
  genera_screen_display_notify_information("Notify", "Play/Pause");
  vTaskDelay(500 / portTICK_PERIOD_MS);
  general_screen_display_menu(hid_current_item);
}

void hid_module_display_device_pairing() {
  led_control_run_effect(led_control_ble_spam_breathing);
  genera_screen_display_notify_information("Pairing", "Waiting for connection");
}

void hid_module_display_device_connection(bool status) {
  led_control_stop();
  if (status) {
    genera_screen_display_notify_information("Notify", "Connected");
  } else {
    general_register_menu(&hid_menu);
    genera_screen_display_notify_information("Notify", "Disconnected");
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  hid_current_item = 0;
  general_screen_display_menu(hid_current_item);
}

void hid_module_display_device_name() {
  led_control_run_effect(led_control_pulse_leds);
  genera_screen_display_card_information("HID Device", "HIDNAME");
}
void hid_module_display_device_mac() {
  led_control_run_effect(led_control_pulse_leds);
  genera_screen_display_card_information("HID Device", "MACADDD");
}

void hid_module_display_menu(uint16_t current_item) {
  led_control_run_effect(led_control_pulse_leds);
  hid_current_item = current_item;
  general_screen_display_menu(current_item);
}