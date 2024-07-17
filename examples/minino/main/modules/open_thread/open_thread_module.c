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
uint8_t channel = 15;

static app_screen_state_information_t app_screen_state_information = {
    .in_app = false,
    .app_selected = 0,
};

static void open_thread_module_app_selector();
static void open_thread_module_state_machine(button_event_t button_pressed);

void open_thread_module_begin(int app_selected) {
  radio_selector_set_thread();
  ESP_LOGI(TAG_OT_MODULE,
           "Initializing OpenThread module screen state machine");
  app_screen_state_information.app_selected = app_selected;

  menu_screens_set_app_state(true, open_thread_module_state_machine);
  oled_screen_clear();
  open_thread_module_app_selector();
};

static void open_thread_module_app_selector() {
  switch (app_screen_state_information.app_selected) {
    case MENU_THREAD_APPS:
      led_control_run_effect(led_control_zigbee_scanning);
      open_thread_screens_display_broadcast_mode(channel);
      thread_broadcast_set_on_msg_recieve_cb(
          open_thread_screens_show_new_message);
      thread_broadcast_init();
      break;
    default:
      break;
  }
}

static void open_thread_module_state_machine(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK) {
    printf("return\n");
    return;
  }

  ESP_LOGI(TAG_OT_MODULE, "OpenThread engine state machine from team: %d %d",
           button_name, button_event);
  switch (app_screen_state_information.app_selected) {
    case MENU_THREAD_APPS:
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
        case BUTTON_BOOT:
        default:
          break;
      }
      break;
    default:
      break;
  }
}
