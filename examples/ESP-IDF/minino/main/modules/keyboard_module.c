#include "keyboard_module.h"
#include "bluetooth_scanner.h"
#include "display.h"
#include "esp_log.h"
#include "esp_zb_switch.h"

static const char* TAG = "keyboard";

static void button_event_cb(void* arg, void* data);
void handle_back();
void handle_selected_item();
void update_previous_layer();
void handle_main_selection();
void handle_applications_selection();
void handle_settings_selection();
void handle_about_selection();
void handle_wifi_apps_selection();
void handle_wifi_sniffer_selection();
void handle_bluetooth_apps_selection();
void handle_zigbee_apps_selection();
void handle_zigbee_spoofing_selection();
void handle_zigbee_switch_selection();
void handle_thread_apps_selection();
void handle_gps_selection();

uint8_t button_event;

void button_init(uint32_t button_num, uint8_t mask) {
  button_config_t btn_cfg = {
      .type = BUTTON_TYPE_GPIO,
      .gpio_button_config =
          {
              .gpio_num = button_num,
              .active_level = BUTTON_ACTIVE_LEVEL,
          },
  };
  button_handle_t btn = iot_button_create(&btn_cfg);
  assert(btn);
  esp_err_t err =
      iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb,
                             (void*) (BUTTON_PRESS_DOWN | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb,
                                (void*) (BUTTON_PRESS_UP | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb,
                                (void*) (BUTTON_PRESS_REPEAT | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb,
                                (void*) (BUTTON_PRESS_REPEAT_DONE | mask));
  err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb,
                                (void*) (BUTTON_SINGLE_CLICK | mask));
  err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb,
                                (void*) (BUTTON_DOUBLE_CLICK | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_START | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_HOLD | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_UP | mask));
  ESP_ERROR_CHECK(err);
}

void keyboard_init() {
  // button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
  button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
  button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
  button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
  button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}

static void button_event_cb(void* arg, void* data) {
  uint8_t button_name =
      (((button_event_t) data) >> 4);  // >> 4 to get the button number
  button_event = ((button_event_t) data) &
                 0x0F;  // & 0x0F to get the event number without the mask
  const char* button_name_str = button_names[button_name];
  const char* button_event_str = button_events_table[button_event];
  // if (button_event != BUTTON_PRESS_DOWN) {
  //   return;
  // }
  ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

  switch (button_name) {
    case BUTTON_BOOT:
      break;
    case BUTTON_LEFT:
      if (button_event == BUTTON_PRESS_DOWN)
        handle_back();
      break;
    case BUTTON_RIGHT:
      if (button_event == BUTTON_PRESS_DOWN)
        handle_selected_item();
      if (button_event == BUTTON_PRESS_UP &&
          current_layer == LAYER_ZIGBEE_SWITCH)
        handle_selected_item();
      break;
    case BUTTON_UP:
      if (button_event == BUTTON_PRESS_DOWN) {
        selected_item = (selected_item == 0) ? 0 : selected_item - 1;
        display_menu();
      }
      break;
    case BUTTON_DOWN:
      if (button_event == BUTTON_PRESS_DOWN) {
        selected_item = (selected_item == num_items - 3) ? selected_item
                                                         : selected_item + 1;
        display_menu();
      }
      break;
  }

  if (button_event == BUTTON_PRESS_DOWN) {
    ESP_LOGI(TAG, "Selected item: %d", selected_item);
    ESP_LOGI(TAG, "Menu items: %d", num_items);
    ESP_LOGI(TAG, "Current layer: %d", current_layer);
  }
  update_previous_layer();
}

void handle_back() {
  switch (current_layer) {
    case LAYER_WIFI_SNIFFER_START:
      wifi_sniffer_stop();
      break;
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      if (bluetooth_scanner_is_active()) {
        bluetooth_scanner_stop();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait for the scanner to stop
      break;
    case LAYER_THREAD_CLI:
      // thread_cli_stop();
      break;
    default:
      break;
  }

  current_layer = previous_layer;
  selected_item = 0;
  display_menu();
}

void handle_selected_item() {
  switch (current_layer) {
    case LAYER_MAIN_MENU:
      handle_main_selection();
      break;
    case LAYER_APPLICATIONS:
      handle_applications_selection();
      break;
    case LAYER_SETTINGS:
      handle_settings_selection();
      break;
    case LAYER_ABOUT:
      handle_about_selection();
      break;
    case LAYER_WIFI_APPS:
      handle_wifi_apps_selection();
      break;
    case LAYER_BLUETOOTH_APPS:
      handle_bluetooth_apps_selection();
      break;
    case LAYER_ZIGBEE_APPS:
      handle_zigbee_apps_selection();
      break;
    case LAYER_THREAD_APPS:
      handle_thread_apps_selection();
      break;
    case LAYER_MATTER_APPS:
      break;
    case LAYER_GPS:
      handle_gps_selection();
      break;
    case LAYER_WIFI_SNIFFER:
      handle_wifi_sniffer_selection();
      break;
    case LAYER_ZIGBEE_SPOOFING:
      handle_zigbee_spoofing_selection();
      break;
    case LAYER_ZIGBEE_SWITCH:
      handle_zigbee_switch_selection();
      break;
    default:
      ESP_LOGE(TAG, "Invalid layer");
      break;
  }

  selected_item = 0;
  display_menu();
}

void update_previous_layer() {
  switch (current_layer) {
    case LAYER_MAIN_MENU:
    case LAYER_APPLICATIONS:
    case LAYER_SETTINGS:
    case LAYER_ABOUT:
      previous_layer = LAYER_MAIN_MENU;
      break;
    case LAYER_WIFI_APPS:
    case LAYER_BLUETOOTH_APPS:
    case LAYER_ZIGBEE_APPS:
    case LAYER_THREAD_APPS:
    case LAYER_MATTER_APPS:
    case LAYER_GPS:
      previous_layer = LAYER_APPLICATIONS;
      break;
    case LAYER_ABOUT_VERSION:
    case LAYER_ABOUT_LICENSE:
    case LAYER_ABOUT_CREDITS:
    case LAYER_ABOUT_LEGAL:
      previous_layer = LAYER_ABOUT;
      break;
    case LAYER_SETTINGS_DISPLAY:
    case LAYER_SETTINGS_SOUND:
    case LAYER_SETTINGS_SYSTEM:
      previous_layer = LAYER_SETTINGS;
      break;
    /* WiFi apps */
    case LAYER_WIFI_SNIFFER:
      previous_layer = LAYER_WIFI_APPS;
      break;
    /* WiFi sniffer apps */
    case LAYER_WIFI_SNIFFER_START:
    case LAYER_WIFI_SNIFFER_SETTINGS:
      previous_layer = LAYER_WIFI_SNIFFER;
      break;
    /* Bluetooth apps */
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      previous_layer = LAYER_BLUETOOTH_APPS;
      break;
    case LAYER_ZIGBEE_SPOOFING:
      previous_layer = LAYER_ZIGBEE_APPS;
      break;
    case LAYER_ZIGBEE_SWITCH:
    case LAYER_ZIGBEE_LIGHT:
      previous_layer = LAYER_ZIGBEE_SPOOFING;
      break;
    /* GPS apps */
    case LAYER_GPS_DATE_TIME:
    case LAYER_GPS_LOCATION:
      previous_layer = LAYER_GPS;
      break;
    default:
      ESP_LOGE(TAG, "Unable to update previous layer, current layer: %d",
               current_layer);
      break;
  }
}

void handle_main_selection() {
  switch (selected_item) {
    case MAIN_MENU_APPLICATIONS:
      current_layer = LAYER_APPLICATIONS;
      break;
    case MAIN_MENU_SETTINGS:
      current_layer = LAYER_SETTINGS;
      break;
    case MAIN_MENU_ABOUT:
      current_layer = LAYER_ABOUT;
      break;
  }
}

void handle_applications_selection() {
  switch (selected_item) {
    case APPLICATIONS_MENU_WIFI:
      current_layer = LAYER_WIFI_APPS;
      break;
    case APPLICATIONS_MENU_BLUETOOTH:
      current_layer = LAYER_BLUETOOTH_APPS;
      break;
    case APPLICATIONS_MENU_ZIGBEE:
      current_layer = LAYER_ZIGBEE_APPS;
      break;
    case APPLICATIONS_MENU_THREAD:
      current_layer = LAYER_THREAD_APPS;
      break;
    case APPLICATIONS_MENU_MATTER:
      current_layer = LAYER_MATTER_APPS;
      display_clear();
      display_in_development_banner();
      break;
    case APPLICATIONS_MENU_GPS:
      current_layer = LAYER_GPS;
      break;
  }
}

void handle_settings_selection() {
  switch (selected_item) {
    case SETTINGS_MENU_DISPLAY:
      current_layer = LAYER_SETTINGS_DISPLAY;
      display_clear();
      display_in_development_banner();
      break;
    case SETTINGS_MENU_SOUND:
      current_layer = LAYER_SETTINGS_SOUND;
      display_clear();
      display_in_development_banner();
      break;
    case SETTINGS_MENU_SYSTEM:
      current_layer = LAYER_SETTINGS_SYSTEM;
      display_clear();
      display_in_development_banner();
      break;
  }
}

void handle_about_selection() {
  switch (selected_item) {
    case ABOUT_MENU_VERSION:
      current_layer = LAYER_ABOUT_VERSION;
      break;
    case ABOUT_MENU_LICENSE:
      current_layer = LAYER_ABOUT_LICENSE;
      break;
    case ABOUT_MENU_CREDITS:
      current_layer = LAYER_ABOUT_CREDITS;
      break;
    case ABOUT_MENU_LEGAL:
      current_layer = LAYER_ABOUT_LEGAL;
      break;
  }
}

void handle_wifi_apps_selection() {
  switch (selected_item) {
    case WIFI_MENU_SNIFFER:
      current_layer = LAYER_WIFI_SNIFFER;
      break;
  }
}

void handle_wifi_sniffer_selection() {
  switch (selected_item) {
    case WIFI_SNIFFER_START:
      current_layer = LAYER_WIFI_SNIFFER_START;
      display_clear();
      wifi_sniffer_start();
      break;
    case WIFI_SNIFFER_SETTINGS:
      current_layer = LAYER_WIFI_SNIFFER_SETTINGS;
      break;
    default:
      ESP_LOGE(TAG, "Invalid item: %d", selected_item);
      break;
  }
}

void handle_bluetooth_apps_selection() {
  switch (selected_item) {
    case BLUETOOTH_MENU_AIRTAGS_SCAN:
      current_layer = LAYER_BLUETOOTH_AIRTAGS_SCAN;
      display_clear();
      bluetooth_scanner_start();
      break;
  }
}

void handle_zigbee_apps_selection() {
  switch (selected_item) {
    case ZIGBEE_MENU_SPOOFING:
      current_layer = LAYER_ZIGBEE_SPOOFING;
      break;
  }
}

void handle_zigbee_spoofing_selection() {
  switch (selected_item) {
    case ZIGBEE_SPOOFING_SWITCH:
      current_layer = LAYER_ZIGBEE_SWITCH;
      break;
    case ZIGBEE_SPOOFING_LIGHT:
      current_layer = LAYER_ZIGBEE_LIGHT;
      display_clear();
      display_in_development_banner();
      break;
  }
}

void handle_zigbee_switch_selection() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  switch (selected_item) {
    case ZIGBEE_SWITCH_TOGGLE:
      current_layer = LAYER_ZIGBEE_SWITCH;
      if (button_event == BUTTON_PRESS_DOWN) {
        ESP_LOGI(TAG, "Button pressed");
        display_zb_switch_toggle_pressed();
      } else if (button_event == BUTTON_PRESS_UP) {
        ESP_LOGI(TAG, "Button released");
        display_zb_switch_toggle_released();
        zb_switch_toggle();
      }
      break;
  }
}

void handle_thread_apps_selection() {
  switch (selected_item) {
    case THREAD_MENU_CLI:
      current_layer = LAYER_THREAD_CLI;
      // display_thread_cli();
      break;
  }
}

void handle_gps_selection() {
  switch (selected_item) {
    case GPS_MENU_DATE_TIME:
      current_layer = LAYER_GPS_DATE_TIME;
      break;
    case GPS_MENU_LOCATION:
      current_layer = LAYER_GPS_LOCATION;
      break;
  }
}