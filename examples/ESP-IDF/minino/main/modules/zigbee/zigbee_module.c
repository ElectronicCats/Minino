#include "modules/zigbee/zigbee_module.h"
#include "esp_log.h"
#include "ieee_sniffer.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "modules/zigbee/zigbee_screens_module.h"
#include "oled_screen.h"

static app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = MENU_ZIGBEE_SNIFFER,
};
static int trackers_count = 0;
static int device_selection = 0;
static int current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;
static TaskHandle_t zigbee_task_display_records = NULL;
static TaskHandle_t zigbee_task_display_animation = NULL;
static TaskHandle_t zigbee_task_sniffer = NULL;

static void zigbee_module_app_selector();
static void zigbee_module_state_machine(button_event_t button_pressed);

void zigbee_module_begin(int app_selected) {
  ESP_LOGI(TAG_ZIGBEE_MODULE, "Initializing ble module screen state machine");
  app_screen_state_information.app_selected = app_selected;

  module_keyboard_update_state(true, zigbee_module_state_machine);
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  zigbee_module_app_selector();
};

static void zigbee_module_app_selector() {
  switch (app_screen_state_information.app_selected) {
    case MENU_ZIGBEE_SNIFFER:
      xTaskCreate(zigbee_screens_display_scanning_animation,
                  "zigbee_module_scanning", 4096, NULL, 5,
                  &zigbee_task_display_animation);
      ieee_sniffer_register_cb(zigbee_screens_display_scanning_text);
      xTaskCreate(ieee_sniffer_begin, "ieee_sniffer_task", 4096, NULL, 5,
                  &zigbee_task_sniffer);

      zigbee_screens_display_scanning_text(0, current_channel);
      led_control_run_effect(led_control_zigbee_scanning);
      break;
    default:
      break;
  }
}

static void zigbee_module_state_machine(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK &&
      button_event != BUTTON_LONG_PRESS_HOLD) {
    return;
  }

  ESP_LOGI(TAG_ZIGBEE_MODULE, "Zigbee engine state machine from team: %d %d",
           button_name, button_event);
  switch (app_screen_state_information.app_selected) {
    case MENU_ZIGBEE_SNIFFER:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Bluetooth scanner entered");
      switch (button_name) {
        case BUTTON_LEFT:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button left pressed");
          led_control_stop();
          vTaskDelete(zigbee_task_display_animation);
          ieee_sniffer_stop();
          vTaskDelete(zigbee_task_sniffer);
          module_keyboard_update_state(false, NULL);
          menu_screens_exit_submenu();
          break;
        case BUTTON_RIGHT:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button right pressed - Option selected");
          break;
        case BUTTON_UP:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button up pressed");
          current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MAX)
                                ? IEEE_SNIFFER_CHANNEL_MIN
                                : (current_channel + 1);
          ieee_sniffer_set_channel(current_channel);
          zigbee_screens_display_scanning_text(0, current_channel);
          break;
        case BUTTON_DOWN:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button down pressed");
          current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MIN)
                                ? IEEE_SNIFFER_CHANNEL_MAX
                                : (current_channel - 1);
          ieee_sniffer_set_channel(current_channel);
          zigbee_screens_display_scanning_text(0, current_channel);
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
