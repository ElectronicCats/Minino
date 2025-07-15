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

#include "bitmaps_general.h"
#include "general_notification.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"

#include "zb_cli.h"

#include "general_interact_screen.h"

#define ZIGBEE_SNIFFER_FS_CHAN_KEY "zschan"

static char* menu_main_items[] = {"Channel", "Run"};
static const char* channel_item[] = {"11", "12", "13", "14", "15", "16",
                                     "17", "18", "19", "20", "21", "22",
                                     "23", "24", "25", "26"};

static uint16_t last_index_selected = 0;
static bool is_running = false;

static void zigbee_modue_show_main();
static void zigbee_module_show_channel_selector();

typedef enum {
  ZM_CHANNEL,
  ZM_RUN,
} main_menu_items_t;

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
  update_interactive_screen();
  for (int i = 0; i < packet_length; i++) {
    printf("%02x", packet[i]);
  }
  printf("\n");
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

static void zigbee_module_channel_selector(uint8_t option) {
  preferences_put_int(ZIGBEE_SNIFFER_FS_CHAN_KEY, option + 11);
  zigbee_modue_show_main();
}

static void zigbee_module_show_run_screen() {
  general_interactive_screen_t screen = {0};
  screen.static_text = "Channel";
  screen.dinamic_text = "Packets";
  screen.header_title = "ZB Sniffer";
  screen.select_back_cb = zigbee_modue_show_main;
  screen.select_up_cb = ieee_sniffer_set_channel;
  screen.select_down_cb = ieee_sniffer_set_channel;
  screen.range_low = 11;
  screen.range_high = 26;
  screen.dinamic_value = &packet_count;
  screen.selected_value = preferences_get_int(ZIGBEE_SNIFFER_FS_CHAN_KEY, 11);
  interactive_screen(screen);
}

static void zigbee_module_main_handler(uint8_t option) {
  last_index_selected = 0;
  switch (option) {
    case ZM_CHANNEL:
      zigbee_module_show_channel_selector();
      break;
    case ZM_RUN:
      if (!is_running) {
        led_control_run_effect(led_control_zigbee_scanning);
        xTaskCreate(ieee_sniffer_begin, "ieee_sniffer_task", 4096, NULL, 5,
                    &zigbee_task_sniffer);
        is_running = true;
      }
      zigbee_module_show_run_screen();
      break;
    default:
      break;
  }
}

static void zigbee_module_show_channel_selector(void) {
  general_radio_selection_menu_t channel = {0};
  channel.banner = "Channel";
  channel.options = (char**) channel_item;
  channel.options_count = sizeof(channel_item) / sizeof(char*);
  channel.select_cb = zigbee_module_channel_selector;
  channel.style = RADIO_SELECTION_OLD_STYLE;
  channel.exit_cb = zigbee_modue_show_main;
  channel.current_option =
      preferences_get_int(ZIGBEE_SNIFFER_FS_CHAN_KEY, 11) - 11;
  general_radio_selection(channel);  // Show the radio menu
}

static void zigbee_modue_show_main() {
  general_submenu_menu_t main = {0};
  main.options = menu_main_items;
  main.options_count = sizeof(menu_main_items) / sizeof(char*);
  main.select_cb = zigbee_module_main_handler;
  main.selected_option = last_index_selected;
  main.exit_cb = menus_module_restart;
  general_submenu(main);
}

static void zigbee_module_show_disable_cli(void) {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 4000;
  notification.head = "Warning";
  notification.body = "Please disable the Zigbee CLI before running ZB apps";
  general_notification(notification);
  menus_module_set_reset_screen(MENU_SETTINGS_SYSTEM);
}

void zigbee_module_sniffer_enter() {
  radio_selector_set_zigbee_sniffer();
  ieee_sniffer_register_cb(zigbee_module_display_records_cb);
  if (preferences_get_int("ZBCLI", 0) == 1) {
    zigbee_module_show_disable_cli();
    menus_module_reset();
    return;
  }
  zigbee_modue_show_main();
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
          menus_module_set_reset_screen(MENU_ZIGBEE_SPOOFING);
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
        menus_module_set_reset_screen(MENU_ZIGBEE_APPS);
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
