#include "zigbee_module.h"
#include "esp_log.h"
#include "ieee_sniffer.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "preferences.h"
#include "radio_selector.h"
#include "uart_sender.h"
#include "zigbee_screens_module.h"
#include "zigbee_switch.h"

app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};
static int packet_count = 0;
int current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;
static TaskHandle_t zigbee_task_display_records = NULL;
static TaskHandle_t zigbee_task_display_animation = NULL;
static TaskHandle_t zigbee_task_sniffer = NULL;

static void zigbee_module_app_selector();
static void zigbee_module_state_machine(button_event_state_t button_pressed);

static void zigbee_module_display_records_cb(uint8_t* packet,
                                             uint8_t packet_length) {
  if (packet_count == 1000) {
    packet_count = 0;
  }
  packet_count++;
  zigbee_screens_display_scanning_text(packet_count);
  uart_sender_send_packet(UART_SENDER_PACKET_TYPE_ZIGBEE, packet,
                          packet_length);
}

void zigbee_module_begin(int app_selected) {
#if !defined(CONFIG_ZIGBEE_MODULE_DEBUG)
  esp_log_level_set(TAG_ZIGBEE_MODULE, ESP_LOG_NONE);
#endif

  ESP_LOGI(TAG_ZIGBEE_MODULE,
           "Initializing zigbee module screen state machine");
  app_screen_state_information.app_selected = app_selected;

  menu_screens_set_app_state(true, zigbee_module_state_machine);
  oled_screen_clear(OLED_DISPLAY_NORMAL);
  zigbee_module_app_selector();
};

void zigbee_module_app_selector() {
  switch (app_screen_state_information.app_selected) {
    case MENU_ZIGBEE_SWITCH:
      radio_selector_set_zigbee_switch();
      zigbee_switch_set_display_status_cb(zigbee_screens_module_display_status);
      zigbee_switch_init();
      break;
    case MENU_ZIGBEE_SNIFFER:
      radio_selector_set_zigbee_sniffer();
      zigbee_screens_display_device_ad();
      vTaskDelay(8000 / portTICK_PERIOD_MS);
      ieee_sniffer_register_cb(zigbee_module_display_records_cb);
      xTaskCreate(zigbee_screens_display_scanning_animation,
                  "zigbee_module_scanning", 4096, NULL, 5,
                  &zigbee_task_display_animation);
      xTaskCreate(ieee_sniffer_begin, "ieee_sniffer_task", 4096, NULL, 5,
                  &zigbee_task_sniffer);
      led_control_run_effect(led_control_zigbee_scanning);
      break;
    default:
      break;
  }
}

void zigbee_module_state_machine(button_event_state_t button_pressed) {
  switch (app_screen_state_information.app_selected) {
    case MENU_ZIGBEE_SWITCH:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Zigbee Switch Entered");
      switch (button_pressed.button_pressed) {
        case BUTTON_RIGHT:
          switch (button_pressed.button_event) {
            case BUTTON_PRESS_DOWN:
              if (zigbee_switch_is_light_connected()) {
                zigbee_screens_module_toogle_pressed();
              }
              break;
            case BUTTON_PRESS_UP:
              if (zigbee_switch_is_light_connected()) {
                zigbee_screens_module_toggle_released();
                zigbee_switch_toggle();
              }
              break;
          }
          break;
        case BUTTON_LEFT:
          switch (button_pressed.button_event) {
            case BUTTON_PRESS_DOWN:
              screen_module_set_screen(MENU_ZIGBEE_SWITCH);
              zigbee_switch_deinit();
              break;
          }
          break;
        default:
          break;
      }
      break;
    case MENU_ZIGBEE_SNIFFER:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Zigbee Sniffer Entered");
      switch (button_pressed.button_pressed) {
        case BUTTON_LEFT:
          if (button_pressed.button_event == BUTTON_SINGLE_CLICK) {
            led_control_stop();
            screen_module_set_screen(MENU_ZIGBEE_SNIFFER);
            esp_restart();
          }
          break;
        case BUTTON_RIGHT:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button right pressed - Option selected");
          break;
        case BUTTON_UP:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button up pressed");
          if (button_pressed.button_event == BUTTON_SINGLE_CLICK) {
            current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MAX)
                                  ? IEEE_SNIFFER_CHANNEL_MIN
                                  : (current_channel + 1);
            ieee_sniffer_set_channel(current_channel);
            // zigbee_screens_display_scanning_text(0, current_channel);
          }
          break;
        case BUTTON_DOWN:
          ESP_LOGI(TAG_ZIGBEE_MODULE, "Button down pressed");
          if (button_pressed.button_event == BUTTON_SINGLE_CLICK) {
            current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MIN)
                                  ? IEEE_SNIFFER_CHANNEL_MAX
                                  : (current_channel - 1);
            ieee_sniffer_set_channel(current_channel);
            // zigbee_screens_display_scanning_text(0, current_channel);
          }
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
