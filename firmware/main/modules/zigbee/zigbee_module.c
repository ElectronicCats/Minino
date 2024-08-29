#include "zigbee_module.h"
#include "animations_task.h"
#include "esp_log.h"
#include "ieee_sniffer.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "radio_selector.h"
#include "uart_sender.h"
#include "zigbee_screens_module.h"
#include "zigbee_switch.h"

static int packet_count = 0;
int current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;
static TaskHandle_t zigbee_task_display_records = NULL;
static TaskHandle_t zigbee_task_display_animation = NULL;
static TaskHandle_t zigbee_task_sniffer = NULL;

static void zigbee_module_app_selector();
static void switch_input_cb(uint8_t button_name, uint8_t button_event);
static void sniffer_input_cb(uint8_t button_name, uint8_t button_event);

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
};

void zigbee_module_switch_enter() {
  radio_selector_set_zigbee_switch();
  menus_module_set_app_state(true, switch_input_cb);
  zigbee_switch_set_display_status_cb(zigbee_screens_module_display_status);
  zigbee_switch_init();
}

void zigbee_module_sniffer_enter() {
  radio_selector_set_zigbee_sniffer();
  menus_module_set_app_state(true, sniffer_input_cb);
  zigbee_screens_display_device_ad();
  vTaskDelay(8000 / portTICK_PERIOD_MS);
  ieee_sniffer_register_cb(zigbee_module_display_records_cb);
  zigbee_screens_display_zigbee_sniffer_text();
  animations_task_run(zigbee_screens_display_scanning_animation, 200, NULL);
  xTaskCreate(ieee_sniffer_begin, "ieee_sniffer_task", 4096, NULL, 5,
              &zigbee_task_sniffer);
  led_control_run_effect(led_control_zigbee_scanning);
}

static void switch_input_cb(uint8_t button_name, uint8_t button_event) {
  ESP_LOGI(TAG_ZIGBEE_MODULE, "Zigbee Switch Entered");
  switch (button_name) {
    case BUTTON_RIGHT:
      switch (button_event) {
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
      switch (button_event) {
        case BUTTON_PRESS_DOWN:
          menus_module_set_reset_screen(MENU_ZIGBEE_SPOOFING_2);
          zigbee_switch_deinit();
          break;
      }
      break;
    default:
      break;
  }
}

static void sniffer_input_cb(uint8_t button_name, uint8_t button_event) {
  ESP_LOGI(TAG_ZIGBEE_MODULE, "Zigbee Sniffer Entered");
  switch (button_name) {
    case BUTTON_LEFT:
      if (button_event == BUTTON_SINGLE_CLICK) {
        led_control_stop();
        menus_module_set_reset_screen(MENU_ZIGBEE_APPS_2);
        esp_restart();
      }
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Button right pressed - Option selected");
      break;
    case BUTTON_UP:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Button up pressed");
      if (button_event == BUTTON_SINGLE_CLICK) {
        current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MAX)
                              ? IEEE_SNIFFER_CHANNEL_MIN
                              : (current_channel + 1);
        ieee_sniffer_set_channel(current_channel);
        // zigbee_screens_display_scanning_text(0, current_channel);
      }
      break;
    case BUTTON_DOWN:
      ESP_LOGI(TAG_ZIGBEE_MODULE, "Button down pressed");
      if (button_event == BUTTON_SINGLE_CLICK) {
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
}
