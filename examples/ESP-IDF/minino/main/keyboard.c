#include "keyboard.h"

#include "bluetooth_scanner.h"
#include "keyboard_helper.h"
#include "display.h"
#include "esp_log.h"
#include "thread_cli.h"

static const char* TAG = "keyboard";

static void button_event_cb(void* arg, void* data);
void handle_back();
void handle_selected_option();
void update_previous_layer();
void handle_main_selection();
void handle_applications_selection();
void handle_settings_selection();
void handle_about_selection();
void handle_wifi_apps_selection();
void handle_bluetooth_apps_selection();
void handle_thread_apps_selection();
void handle_gps_selection();

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
  button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
  button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
  button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
  button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
  button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}

static void button_event_cb(void* arg, void* data) {
  uint8_t button_name =
      (((button_event_t) data) >> 4);  // >> 4 to get the button number
  uint8_t button_event =
      ((button_event_t) data) &
      0x0F;  // & 0x0F to get the event number without the mask
  const char* button_name_str = button_name_table[button_name];
  const char* button_event_str = button_event_table[button_event];
  ESP_LOGI("", "");
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

  switch (button_name) {
    case BOOT:
      break;
    case LEFT:
      if (button_event == BUTTON_PRESS_DOWN)
        handle_back();
      break;
    case RIGHT:
      if (button_event == BUTTON_PRESS_DOWN)
        handle_selected_option();
      break;
    case UP:
      if (button_event == BUTTON_PRESS_DOWN) {
        selected_option = (selected_option == 0) ? 0 : selected_option - 1;
        display_menu();
      }
      break;
    case DOWN:
      if (button_event == BUTTON_PRESS_DOWN) {
        selected_option = (selected_option == num_items - 3)
                              ? selected_option
                              : selected_option + 1;
        display_menu();
      }
      break;
  }

  ESP_LOGI(TAG, "Selected option: %d", selected_option);
  ESP_LOGI(TAG, "Options length: %d", num_items);
  ESP_LOGI(TAG, "Current layer: %d", current_layer);
  update_previous_layer();
}

void handle_back() {
  switch (current_layer) {
    case LAYER_WIFI_ANALIZER:
      // wifi_sniffer_deinit();
      break;
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      if (bluetooth_scanner_is_active()) {
        bluetooth_scanner_stop();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait for the scanner to stop
      break;
    case LAYER_THREAD_CLI:
      thread_cli_stop();
      break;
    default:
      break;
  }

  current_layer = previous_layer;
  selected_option = 0;
  display_menu();
}

void handle_selected_option() {
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
      break;
    case LAYER_THREAD_APPS:
      handle_thread_apps_selection();
      break;
    case LAYER_MATTER_APPS:
      break;
    case LAYER_GPS:
      handle_gps_selection();
      break;
    case LAYER_WIFI_ANALIZER:
      break;
    default:
      ESP_LOGE(TAG, "Invalid layer");
      break;
  }

  selected_option = 0;
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
    /* WiFi applications */
    case LAYER_WIFI_ANALIZER:
      previous_layer = LAYER_WIFI_APPS;
      break;
    /* Bluetooth applications */
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      previous_layer = LAYER_BLUETOOTH_APPS;
      break;
    /* GPS applications */
    case LAYER_GPS_DATE_TIME:
    case LAYER_GPS_LOCATION:
      previous_layer = LAYER_GPS;
      break;
    default:
      ESP_LOGE(TAG, "Invalid layer");
      break;
  }
}

void handle_main_selection() {
  switch (selected_option) {
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
  switch (selected_option) {
    case APPLICATIONS_MENU_WIFI:
      current_layer = LAYER_WIFI_APPS;
      break;
    case APPLICATIONS_MENU_BLUETOOTH:
      current_layer = LAYER_BLUETOOTH_APPS;
      break;
    case APPLICATIONS_MENU_ZIGBEE:
      current_layer = LAYER_ZIGBEE_APPS;
      display_clear();
      display_in_development_banner();
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
  switch (selected_option) {
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
  switch (selected_option) {
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
  switch (selected_option) {
    case WIFI_MENU_ANALIZER:
      current_layer = LAYER_WIFI_ANALIZER;
      display_clear();
      // wifi_sniffer_init();
      break;
  }
}

void handle_bluetooth_apps_selection() {
  switch (selected_option) {
    case BLUETOOTH_MENU_AIRTAGS_SCAN:
      current_layer = LAYER_BLUETOOTH_AIRTAGS_SCAN;
      display_clear();
      bluetooth_scanner_start();
      break;
  }
}

void handle_thread_apps_selection() {
  switch (selected_option) {
    case THREAD_MENU_CLI:
      current_layer = LAYER_THREAD_CLI;
      display_thread_cli();
      break;
  }
}

void handle_gps_selection() {
  switch (selected_option) {
    case GPS_MENU_DATE_TIME:
      current_layer = LAYER_GPS_DATE_TIME;
      break;
    case GPS_MENU_LOCATION:
      current_layer = LAYER_GPS_LOCATION;
      break;
  }
}
