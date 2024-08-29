#include "open_thread_module.h"
#include "esp_log.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "open_thread.h"
#include "open_thread_screens_module.h"
#include "preferences.h"
#include "radio_selector.h"
#include "thread_broadcast.h"
#include "thread_sniffer.h"
#include "thread_sniffer_screens.h"
uint8_t channel = 15;

static void thread_broadcast_input(uint8_t button_name, uint8_t button_event);
static void thread_sniffer_input(uint8_t button_name, uint8_t button_event);

void open_thread_module_begin() {
#if !defined(CONFIG_OPEN_THREAD_MODULE_DEBUG)
  esp_log_level_set(TAG_OT_MODULE, ESP_LOG_NONE);
#endif
  radio_selector_set_thread();
}

void open_thread_module_exit() {
  menus_module_set_reset_screen(MENU_THREAD_APPS_2);
  esp_restart();
}

void open_thread_module_broadcast_enter() {
  radio_selector_set_thread();
  menus_module_set_app_state(true, thread_broadcast_input);
  led_control_run_effect(led_control_zigbee_scanning);
  open_thread_screens_display_broadcast_mode(channel);
  thread_broadcast_set_on_msg_recieve_cb(open_thread_screens_show_new_message);
  thread_broadcast_init();
}

void open_thread_module_sniffer_enter() {
  radio_selector_set_thread();
  thread_sniffer_set_show_event_cb(thread_sniffer_show_event_handler);
  thread_sniffer_init();
}
void open_thread_module_sniffer_run() {
  menus_module_set_app_state(true, thread_sniffer_input);
  led_control_run_effect(led_control_zigbee_scanning);
  thread_sniffer_run();
}

static void thread_broadcast_input(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      led_control_stop();
      open_thread_module_exit();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      printf("channel++\n");
      channel = ++channel > 26 ? 11 : channel;
      openthread_set_dataset(channel, 0x1234);
      open_thread_screens_display_broadcast_mode(channel);
      break;
    case BUTTON_DOWN:
      printf("channel--\n");
      channel = --channel < 11 ? 26 : channel;
      openthread_set_dataset(channel, 0x1234);
      open_thread_screens_display_broadcast_mode(channel);
      break;
    default:
      break;
  }
}

static void thread_sniffer_input(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      thread_sniffer_stop();
      led_control_stop();
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    default:
      break;
  }
}
