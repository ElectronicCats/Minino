#include "trackers_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general/bitmaps_general.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "oled_screen.h"

static uint16_t hid_current_item = 0;
static uint16_t trackers_current_item = 0;
static char* list_trackers[20];

static const general_menu_t main_menu = {
    .menu_items = trackers_menu_items,
    .menu_count = TRACKERS_COUNT,
    .menu_level = GENERAL_TREE_APP_MENU,
};

static general_menu_t trackers_list = {
    .menu_items = list_trackers,
    .menu_count = 20,
    .menu_level = GENERAL_TREE_APP_SUBMENU,
};

void module_register_menu(menu_tree_t menu) {
  switch (menu) {
    case GENERAL_TREE_APP_MENU:
      general_register_menu(&main_menu);
      break;
    case GENERAL_TREE_APP_SUBMENU:
      general_register_menu(&trackers_list);
      break;
    default:
      general_register_menu(&main_menu);
      break;
  }
}

void module_add_tracker_to_list(char* tracker_name) {
  list_trackers[trackers_current_item] = tracker_name;
  trackers_current_item++;
  trackers_list.menu_count = trackers_current_item;
}

void module_update_scan_state(bool scanning) {
  trackers_menu_items[TRACKERS_SCAN] = scanning ? "SCANNING" : "SCAN";
}

void module_update_tracker_name(char* tracker_name, uint16_t index) {
  list_trackers[index] = tracker_name;
}

void module_display_scanning() {
  led_control_run_effect(led_control_pulse_leds);
  genera_screen_display_notify_information("Searching", "Looking for devices");
}

void module_display_tracker_information(char* title, char* body) {
  led_control_run_effect(led_control_pulse_leds);
  genera_screen_display_card_information(title, body);
}

void module_display_device_detected(char* device_name) {
  led_control_run_effect(led_control_pulse_leds);
  genera_screen_display_notify_information("Device found", device_name);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  general_screen_display_menu(hid_current_item);
}

void module_display_menu(uint16_t current_item) {
  led_control_run_effect(led_control_pulse_leds);
  hid_current_item = current_item;
  general_screen_display_menu(current_item);
}