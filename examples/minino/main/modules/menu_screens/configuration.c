#include "esp_log.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "preferences.h"

#define TAG_CONFIG_MODULE "CONFIG_MODULE"

static app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};

static void config_module_app_selector();
static void config_module_state_machine(button_event_t button_pressed);

static void config_module_wifi_display_list() {
  int count = preferences_get_int("count_ap", 0);
  if (count == 0) {
    printf("No saved APs\n");
    return;
  }
  ESP_LOGI(__func__, "Saved APs: %d", count);
  for (int i = 0; i < count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      ESP_LOGW(__func__, "Error getting AP");
      return;
    }
    printf("[%i] SSID: %s\n", i, wifi_ssid);
  }
}

void config_module_begin(int app_selected) {
  ESP_LOGI(TAG_CONFIG_MODULE, "Initializing ble module screen state machine");
  app_screen_state_information.app_selected = app_selected;

  menu_screens_set_app_state(true, config_module_state_machine);
  oled_screen_clear();
  config_module_app_selector();
};

static void config_module_app_selector() {
  led_control_run_effect(led_control_ble_tracking);
  switch (app_screen_state_information.app_selected) {
    case MENU_SETTINGS_WIFI:
      oled_screen_display_text_center("WIFI", 0, OLED_DISPLAY_NORMAL);
      break;
    default:
      break;
  }
}

static void config_module_state_machine(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK &&
      button_event != BUTTON_LONG_PRESS_HOLD) {
    return;
  }

  ESP_LOGI(TAG_CONFIG_MODULE, "BLE engine state machine from team: %d %d",
           button_name, button_event);
  switch (app_screen_state_information.app_selected) {
    case MENU_SETTINGS_WIFI:
      ESP_LOGI(TAG_CONFIG_MODULE, "Bluetooth scanner entered");
      switch (button_name) {
        case BUTTON_LEFT:
        case BUTTON_RIGHT:
        case BUTTON_UP:
        case BUTTON_DOWN:
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    default:
      break;
  }
}
