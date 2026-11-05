#include "open_thread_module.h"
#include "animations_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#include "wifi_service.h"

#define THREAD_SNIFFER_FS_CHAN_KEY "ot_schan"

static uint8_t channel = 15;
static volatile uint16_t ot_sniffer_packet_count = 0;
static volatile bool s_ot_sniffer_running = false;
static volatile bool s_ot_broadcast_running = false;
static TaskHandle_t s_ot_ui_task_handle = NULL;

static void thread_broadcast_input(uint8_t button_name, uint8_t button_event);
static void ot_sniffer_event_handler(thread_sniffer_events_t event, void* ctx);

static void ot_sniffer_ui_task(void* arg) {
  (void) arg;
  uint16_t last_rendered = 0xFFFF;
  while (s_ot_sniffer_running) {
    if (last_rendered != ot_sniffer_packet_count) {
      last_rendered = ot_sniffer_packet_count;
      update_interactive_screen();
    }
    vTaskDelay(pdMS_TO_TICKS(150));
  }
  s_ot_ui_task_handle = NULL;
  vTaskDelete(NULL);
}

static void on_interactive_screen_back(void) {
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
      /* UI update is handled by ot_sniffer_ui_task to avoid starving CPU and
       * stack */
      break;
    case THREAD_SNIFFER_FATAL_ERROR_EV:
      thread_sniffer_show_event_handler(event, ctx);
      break;
    default:
      break;
  }
}

void open_thread_module_begin(void) {
#if !defined(CONFIG_OPEN_THREAD_MODULE_DEBUG)
  esp_log_level_set(TAG_OT_MODULE, ESP_LOG_NONE);
#endif
  radio_selector_set_thread();
}

void open_thread_module_exit(void) {
  if (s_ot_broadcast_running) {
    s_ot_broadcast_running = false;
    thread_broadcast_deinit();
    led_control_stop();
    wifi_service_init(WIFI_MODE_STA);
    return;
  }
  if (!s_ot_sniffer_running) {
    return;
  }
  s_ot_sniffer_running = false;

  thread_sniffer_unsubscribe_show_event(ot_sniffer_event_handler);
  thread_sniffer_stop();
  animations_task_stop();
  led_control_stop();

  for (int i = 0; i < 100 && s_ot_ui_task_handle != NULL; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  s_ot_ui_task_handle = NULL;

  wifi_service_init(WIFI_MODE_STA);
}

void open_thread_module_broadcast_enter(void) {
  if (s_ot_broadcast_running) {
    return;
  }
  wifi_service_deinit();
  radio_selector_set_thread();
  menus_module_set_app_state(true, thread_broadcast_input);
  led_control_run_effect(led_control_zigbee_scanning);
  open_thread_screens_display_broadcast_mode(channel);
  thread_broadcast_set_on_msg_recieve_cb(open_thread_screens_show_new_message);
  thread_broadcast_init();
  s_ot_broadcast_running = true;
}

void open_thread_module_sniffer_enter(void) {
  if (s_ot_sniffer_running) {
    return;
  }
  wifi_service_deinit();
  radio_selector_set_thread();
  ot_sniffer_packet_count = 0;
  thread_sniffer_subscribe_show_event(ot_sniffer_event_handler);
  thread_sniffer_init();

  int saved_channel = preferences_get_int(THREAD_SNIFFER_FS_CHAN_KEY, 11);

  general_interactive_screen_t screen = {0};
  screen.header_title = "TH Sniffer";
  screen.static_text = "Channel";
  screen.dinamic_text = "Packets";
  screen.range_low = 11;
  screen.range_high = 26;
  screen.selected_value = saved_channel;
  screen.dinamic_value = (uint16_t*) &ot_sniffer_packet_count;
  screen.select_back_cb = on_interactive_screen_back;
  screen.select_up_cb = ot_sniffer_set_channel;
  screen.select_down_cb = ot_sniffer_set_channel;
  interactive_screen(screen);

  led_control_run_effect(led_control_zigbee_scanning);
  thread_sniffer_set_channel(saved_channel);
  s_ot_sniffer_running = true;
  xTaskCreate(ot_sniffer_ui_task, "ot_ui_task", 3072, NULL, 4,
              &s_ot_ui_task_handle);
  thread_sniffer_run();
}

void open_thread_module_sniffer_run(void) {
  open_thread_module_sniffer_enter();
}

static void thread_broadcast_input(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
    case BUTTON_BOOT:
      s_ot_broadcast_running = false;
      thread_broadcast_deinit();
      led_control_stop();
      wifi_service_init(WIFI_MODE_STA);
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      printf("channel++\n");
      channel = (channel + 1 > 26) ? 11 : (channel + 1);
      openthread_set_dataset(channel, 0x1234);
      open_thread_screens_display_broadcast_mode(channel);
      break;
    case BUTTON_DOWN:
      printf("channel--\n");
      channel = (channel - 1 < 11) ? 26 : (channel - 1);
      openthread_set_dataset(channel, 0x1234);
      open_thread_screens_display_broadcast_mode(channel);
      break;
    default:
      break;
  }
}
