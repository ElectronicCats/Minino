#include "open_thread_module.h"
#include "esp_log.h"
#include "led_events.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "open_thread.h"
#include "open_thread_screens_module.h"
#include "preferences.h"
#include "radio_selector.h"
#include "thread_broadcast.h"
#include "thread_sniffer.h"
uint8_t channel = 15;

static app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};

static void open_thread_module_app_selector();
static void thread_broadcast_input(button_event_t button_pressed);
static void thread_sniffer_input(button_event_t button_pressed);
void open_thread_module_exit_submenu_cb();

void open_thread_module_begin(int app_selected) {
#if !defined(CONFIG_OPEN_THREAD_MODULE_DEBUG)
  esp_log_level_set(TAG_OT_MODULE, ESP_LOG_NONE);
#endif
  radio_selector_set_thread();
  app_screen_state_information.app_selected = app_selected;
  open_thread_module_app_selector();
};

static void open_thread_module_app_selector() {
  oled_screen_clear();
  switch (app_screen_state_information.app_selected) {
    case MENU_THREAD_BROADCAST:
      menu_screens_set_app_state(true, thread_broadcast_input);
      led_control_run_effect(led_control_zigbee_scanning);
      open_thread_screens_display_broadcast_mode(channel);
      thread_broadcast_set_on_msg_recieve_cb(
          open_thread_screens_show_new_message);
      thread_broadcast_init();
      break;
    case MENU_THREAD_SNIFFER:
      menu_screens_register_exit_submenu_cb(open_thread_module_exit_submenu_cb);
      thread_sniffer_init();
      break;
    case MENU_THREAD_SNIFFER_RUN:
      menu_screens_set_app_state(true, thread_sniffer_input);
      led_control_run_effect(led_control_zigbee_scanning);
      // SET CB
      thread_sniffer_run();
      break;
    default:
      break;
  }
}

void open_thread_module_exit_submenu_cb() {
  screen_module_menu_t current_menu = menu_screens_get_current_menu();
  switch (current_menu) {
    case MENU_THREAD_SNIFFER:
      screen_module_set_screen(MENU_THREAD_SNIFFER);
      esp_restart();
      break;
    default:
      break;
  }
}

static void thread_broadcast_input(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      led_control_stop();
      screen_module_set_screen(MENU_THREAD_APPS);
      esp_restart();
      break;
    case BUTTON_RIGHT:
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

static void thread_sniffer_input(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menu_screens_set_app_state(false, NULL);
      thread_sniffer_stop();
      led_control_stop();
      menu_screens_exit_submenu();
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
