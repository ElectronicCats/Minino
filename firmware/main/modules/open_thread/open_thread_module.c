#include "open_thread_module.h"
#include "esp_log.h"
#include "general_interact_screen.h"
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

#define THREAD_SNIFFER_FS_CHAN_KEY "ot_schan"

uint8_t channel = 15;
static uint16_t ot_sniffer_packet_count = 0;

static void thread_broadcast_input(uint8_t button_name, uint8_t button_event);

static void ot_sniffer_stop_and_exit() {
  thread_sniffer_stop();
  led_control_stop();
  menus_module_exit_app();
}

static void ot_sniffer_set_channel(uint8_t ch) {
  ot_sniffer_packet_count = 0;
  preferences_put_int(THREAD_SNIFFER_FS_CHAN_KEY, ch);
  thread_sniffer_set_channel(ch);
}

static void ot_sniffer_event_handler(thread_sniffer_events_t event, void* ctx) {
  switch (event) {
    case THREAD_SNIFFER_NEW_PACKET_EV:
      ot_sniffer_packet_count = (uint16_t) (*(uint32_t*) ctx);
      update_interactive_screen();
      break;
    case THREAD_SNIFFER_FATAL_ERROR_EV:
      thread_sniffer_show_event_handler(event, ctx);
      break;
    default:
      break;
  }
}

void open_thread_module_begin() {
#if !defined(CONFIG_OPEN_THREAD_MODULE_DEBUG)
  esp_log_level_set(TAG_OT_MODULE, ESP_LOG_NONE);
#endif
  radio_selector_set_thread();
}

void open_thread_module_exit() {
  menus_module_set_reset_screen(MENU_THREAD_APPS);
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
  ot_sniffer_packet_count = 0;
  thread_sniffer_set_show_event_cb(ot_sniffer_event_handler);
  thread_sniffer_init();
}

void open_thread_module_sniffer_run() {
  ot_sniffer_packet_count = 0;
  int saved_channel = preferences_get_int(THREAD_SNIFFER_FS_CHAN_KEY, 11);

  general_interactive_screen_t screen = {0};
  screen.header_title = "TH Sniffer";
  screen.static_text = "Channel";
  screen.dinamic_text = "Packets";
  screen.range_low = 11;
  screen.range_high = 26;
  screen.selected_value = saved_channel;
  screen.dinamic_value = &ot_sniffer_packet_count;
  screen.select_back_cb = ot_sniffer_stop_and_exit;
  screen.select_up_cb = ot_sniffer_set_channel;
  screen.select_down_cb = ot_sniffer_set_channel;
  interactive_screen(screen);

  led_control_run_effect(led_control_zigbee_scanning);
  thread_sniffer_set_channel(saved_channel);
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
