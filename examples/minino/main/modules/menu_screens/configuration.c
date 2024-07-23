#include <string.h>
#include "cmd_wifi.h"
#include "esp_log.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "preferences.h"

#define TAG_CONFIG_MODULE "CONFIG_MODULE"

static int selected_item = 0;
static int total_items = 0;
static int max_items = 6;

static app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};

static void config_module_app_selector();
static void config_module_state_machine(button_event_t button_pressed);
static void config_module_wifi_display_list();
static void config_module_wifi_display_connection_status();

static void config_module_wifi_display_connection_status() {
  oled_screen_clear();
  oled_screen_display_text_center("Connecting to WIFI", 4, OLED_DISPLAY_NORMAL);
}

static void config_module_wifi_display_list() {
  oled_screen_clear();
  oled_screen_display_text_center("Selected WIFI", 0, OLED_DISPLAY_NORMAL);

  for (int i = selected_item; i < max_items + selected_item; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      ESP_LOGW(__func__, "Error getting AP");
      return;
    }
    char wifi_text[120];
    if (strlen(wifi_ssid) > 16) {
      wifi_ssid[16] = '\0';
    }
    if (i == selected_item) {
      sprintf(wifi_text, "[>] %s", wifi_ssid);
    } else {
      sprintf(wifi_text, "[] %s", wifi_ssid);
    }
    int page = (i + 1) - selected_item;
    oled_screen_display_text(
        wifi_text, 0, page,
        (selected_item == i) ? OLED_DISPLAY_INVERT : OLED_DISPLAY_NORMAL);
  }
}

void config_module_begin(int app_selected) {
#if !defined(CONFIG_CONFIGURATION_DEBUG)
  esp_log_level_set(TAG_CONFIG_MODULE, ESP_LOG_NONE);
#endif

  ESP_LOGI(TAG_CONFIG_MODULE, "Initializing ble module screen state machine");
  app_screen_state_information.app_selected = app_selected;

  menu_screens_set_app_state(true, config_module_state_machine);
  oled_screen_clear();
  config_module_app_selector();
};

static void config_module_app_selector() {
  switch (app_screen_state_information.app_selected) {
    case MENU_SETTINGS_WIFI:
      int count = preferences_get_int("count_ap", 0);
      if (count == 0) {
        oled_screen_display_text_center("No saved APs", 0, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("Add new AP", 1, OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("From our serial", 2,
                                        OLED_DISPLAY_NORMAL);
        oled_screen_display_text_center("Console", 3, OLED_DISPLAY_NORMAL);
        return;
      }
      total_items = count;
      ESP_LOGI(__func__, "Saved APs: %d", count);
      config_module_wifi_display_list();
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
          menu_screens_set_app_state(false, NULL);
          menu_screens_exit_submenu();
          break;
        case BUTTON_RIGHT:
          ESP_LOGI(TAG_CONFIG_MODULE, "Selected item: %d", selected_item);
          char wifi_ap[100];
          char wifi_ssid[100];
          sprintf(wifi_ap, "wifi%d", selected_item);
          esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
          if (err != ESP_OK) {
            ESP_LOGW(__func__, "Error getting AP");
            return;
          }
          ESP_LOGI(TAG_CONFIG_MODULE, "Selected AP: %s", wifi_ssid);
          char wifi_password[100];
          esp_err_t err_pass =
              preferences_get_string(wifi_ssid, wifi_password, 100);
          if (err_pass != ESP_OK) {
            ESP_LOGW(__func__, "Error getting AP password");
            return;
          }
          int connection = connect_wifi(wifi_ssid, wifi_password, NULL);
          if (connection == 0) {
            ESP_LOGI(TAG_CONFIG_MODULE, "Connected to AP: %s", wifi_ssid);
          } else {
            ESP_LOGW(TAG_CONFIG_MODULE, "Error connecting to AP: %s",
                     wifi_ssid);
          }
          break;
        case BUTTON_UP:
          selected_item =
              (selected_item == 0) ? total_items - 1 : selected_item - 1;

          config_module_wifi_display_list();
          break;
        case BUTTON_DOWN:
          selected_item =
              (selected_item == total_items - 1) ? 0 : selected_item + 1;
          config_module_wifi_display_list();
          break;
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    default:
      break;
  }
}
