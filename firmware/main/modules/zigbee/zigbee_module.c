#include "zigbee_module.h"
#include <inttypes.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_interact_screen.h"
#include "ieee_sniffer.h"
#include "led_events.h"
#include "menus_module.h"
#include "preferences.h"
#include "radio_selector.h"
#include "uart_sender.h"
#include "wifi_service.h"
#include "zigbee_light.h"
#include "zigbee_screens_module.h"
#include "zigbee_switch.h"

#define ZIGBEE_SNIFFER_FS_CHAN_KEY "zschan"

static volatile uint16_t packet_count = 0;
static TaskHandle_t zigbee_task_sniffer = NULL;
static TaskHandle_t zigbee_ui_task_handle = NULL;
static volatile bool sniffer_running = false;

static void light_input_cb(uint8_t button_name, uint8_t button_event);
static void switch_input_cb(uint8_t button_name, uint8_t button_event);

static void zigbee_module_display_records_cb(uint8_t* packet,
                                             uint8_t packet_length) {
  if (packet_count >= 65535) {
    packet_count = 0;
  }
  packet_count++;
  uart_sender_send_packet(UART_SENDER_PACKET_TYPE_ZIGBEE, packet,
                          packet_length);
}

static void sniffer_ui_task(void* arg) {
  (void) arg;
  uint16_t last_rendered = 0xFFFF;
  while (sniffer_running) {
    if (last_rendered != packet_count) {
      last_rendered = packet_count;
      update_interactive_screen();
    }
    vTaskDelay(pdMS_TO_TICKS(150));
  }
  zigbee_ui_task_handle = NULL;
  vTaskDelete(NULL);
}

void zigbee_module_begin(int app_selected) {
  (void) app_selected;
#if !defined(CONFIG_ZIGBEE_MODULE_DEBUG)
  esp_log_level_set(TAG_ZIGBEE_MODULE, ESP_LOG_NONE);
#endif
}

static void zigbee_module_set_channel(uint8_t channel) {
  packet_count = 0;
  preferences_put_int(ZIGBEE_SNIFFER_FS_CHAN_KEY, channel);
  ieee_sniffer_set_channel(channel);
}

static void on_interactive_screen_back(void) {
  /* Exit through the menus engine so navigation and on_exit_cb are resolved */
  menus_module_exit_app();
}

void zigbee_module_sniffer_enter(void) {
  if (sniffer_running) {
    return;
  }

  /* Release WiFi resources and PHY */
  wifi_service_deinit();

  radio_selector_set_zigbee_sniffer();
  packet_count = 0;
  int saved_channel = preferences_get_int(ZIGBEE_SNIFFER_FS_CHAN_KEY, 11);

  general_interactive_screen_t screen = {0};
  screen.header_title = "ZB Sniffer";
  screen.static_text = "Channel";
  screen.dinamic_text = "Packets";
  screen.range_low = 11;
  screen.range_high = 26;
  screen.selected_value = saved_channel;
  screen.dinamic_value = (uint16_t*) &packet_count;
  screen.select_back_cb = on_interactive_screen_back;
  screen.select_up_cb = zigbee_module_set_channel;
  screen.select_down_cb = zigbee_module_set_channel;
  interactive_screen(screen);

  led_control_run_effect(led_control_zigbee_scanning);
  ieee_sniffer_register_cb(zigbee_module_display_records_cb);
  ieee_sniffer_set_channel(saved_channel);

  sniffer_running = true;
  xTaskCreate(ieee_sniffer_begin, "zb_sniff_task", 3072, NULL, 5,
              &zigbee_task_sniffer);
  xTaskCreate(sniffer_ui_task, "zb_ui_task", 3072, NULL, 4,
              &zigbee_ui_task_handle);
}

void zigbee_module_sniffer_exit(void) {
  if (!sniffer_running) {
    return;
  }
  sniffer_running = false;

  /* Stop IEEE sniffer and unregister callback */
  ieee_sniffer_stop();
  ieee_sniffer_register_cb(NULL);
  led_control_stop();

  /* Allow background tasks to complete self-deletion */
  for (int i = 0; i < 100 && zigbee_ui_task_handle != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelay(pdMS_TO_TICKS(30));
  zigbee_task_sniffer = NULL;
  zigbee_ui_task_handle = NULL;

  /* Restore WiFi in STA mode */
  wifi_service_init(WIFI_MODE_STA);
}

void zigbee_module_switch_enter(void) {
  wifi_service_deinit();
  radio_selector_set_zigbee_switch();
  zigbee_set_app_signal_handler(NULL);
  menus_module_set_app_state(true, switch_input_cb);
  zigbee_switch_set_display_status_cb(zigbee_screens_module_display_status);
  zigbee_switch_init();
}

void zigbee_module_light_enter(void) {
  wifi_service_deinit();
  radio_selector_set_zigbee_light();
  zigbee_set_app_signal_handler(zigbee_light_app_signal_handler);
  menus_module_set_app_state(true, light_input_cb);
  zigbee_light_set_display_cb(zigbee_screens_light_display_status);
  zigbee_light_init();
}

static void light_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name == BUTTON_LEFT || button_name == BUTTON_BOOT) {
    menus_module_set_reset_screen(MENU_ZIGBEE_SPOOFING);
    zigbee_light_deinit();
  }
}

static void switch_input_cb(uint8_t button_name, uint8_t button_event) {
  switch (button_name) {
    case BUTTON_RIGHT:
      if (button_event == BUTTON_PRESS_DOWN) {
        if (zigbee_switch_is_light_connected()) {
          zigbee_screens_module_toogle_pressed();
        }
      } else if (button_event == BUTTON_PRESS_UP) {
        if (zigbee_switch_is_light_connected()) {
          zigbee_screens_module_toggle_released();
          zigbee_switch_toggle();
        }
      }
      break;
    case BUTTON_LEFT:
    case BUTTON_BOOT:
      if (button_event == BUTTON_PRESS_DOWN) {
        menus_module_set_reset_screen(MENU_ZIGBEE_SPOOFING);
        zigbee_switch_deinit();
      }
      break;
    default:
      break;
  }
}
