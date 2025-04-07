#include <string.h>
#include "animations_task.h"
#include "bitmaps_general.h"
#include "cmd_wifi.h"
#include "esp_log.h"
#include "general/general_screens.h"
#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "led_events.h"
#include "menus_module.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wifi_ap_manager.h"
#include "wifi_bitmaps.h"
#include "wifi_settings_scenes.h"

#define TAG_CONFIG_MODULE "CONFIG_MODULE"

static char* wifi_list[20];
static int selected_ap = 0;
static int aps_count = 0;

static char* options_list[] = {"Connect", "Forget"};

static char* option_yn[] = {"No", "Yes"};

typedef enum {
  WIFI_OPT_CONNECT,
  WIFI_OPT_FORGET,
} wifi_options_t;

static void wifi_settings_show_list();

static void wifi_settings_delete_handler() {
  int res = wifi_ap_manager_delete_ap_by_index(selected_ap);
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 2000;
  notification.head = "Forget";
  if (res == 0) {
    notification.body = "Done";
  } else {
    notification.body = "Error";
  }
  general_notification(notification);
  wifi_settings_show_list();
}

static void wifi_settings_yn_handler(uint8_t option) {
  if (option == 1) {
    wifi_settings_delete_handler();
    return;
  }
  wifi_settings_show_list();
}

static void wifi_settings_connecting_animation() {
  oled_screen_clear_buffer();
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t width = 56;
  uint8_t height = 56;
  uint8_t x = (128 - width) / 2;
#else
  uint8_t width = 32;
  uint8_t height = 32;
  uint8_t x = (64 - width) / 2;
  // uint8_t x = 0;
#endif

  static uint8_t idx = 0;
  oled_screen_display_bitmap(epd_bitmap_wifi_loading[idx], x, 1, width, height,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > 3 ? 0 : idx;
  oled_screen_display_show();
}

static void wifi_settings_show_connecting() {
  animations_task_run(&wifi_settings_connecting_animation, 100, NULL);
}

static void wifi_settings_show_connection_cb(bool state) {
  animations_task_stop();
  general_notification_ctx_t notification = {0};

  notification.duration_ms = 2000;
  notification.head = "Wifi";
  if (state) {
    notification.body = "Connected";
  } else {
    notification.body = "Error";
  }
  general_notification(notification);
  wifi_settings_show_list();
}

static void wifi_settings_selected_handler(uint8_t option) {
  switch (option) {
    case WIFI_OPT_CONNECT: {
      wifi_settings_show_connecting();
      wifi_ap_manager_connect_index_cb(selected_ap,
                                       wifi_settings_show_connection_cb);
    }
    case WIFI_OPT_FORGET: {
      general_submenu_menu_t menu_opts = {0};
      menu_opts.options = option_yn;
      menu_opts.options_count = 2;
      menu_opts.select_cb = wifi_settings_yn_handler;
      menu_opts.selected_option = 0;
      menu_opts.exit_cb = wifi_settings_show_list;
      menu_opts.modal = true;
      general_submenu(menu_opts);
    }
  }
}

static void wifi_settings_show_options_list(uint8_t option) {
  selected_ap = option;
  general_submenu_menu_t menu_opts = {0};
  menu_opts.options = options_list;
  menu_opts.options_count = 2;
  menu_opts.select_cb = wifi_settings_selected_handler;
  menu_opts.selected_option = 0;
  menu_opts.exit_cb = wifi_settings_show_list;
  menu_opts.modal = true;
  general_submenu(menu_opts);
}

static void wifi_settings_exit_app() {
  wifi_ap_manager_unregister_callback();
  wifi_settings_scenes_main();
}

static void wifi_settings_show_no_ap() {
  general_notification_ctx_t notification = {0};

  notification.duration_ms = 2000;
  notification.head = "Not found";
  notification.body = "No APs saved";
  general_notification(notification);
  wifi_settings_show_list();
}

static void wifi_settings_show_list() {
  aps_count = preferences_get_int("count_ap", 0);

  if (aps_count == 0) {
    return;
  }
  for (int i = 0; i < aps_count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      continue;
    }
    wifi_list[i] = malloc(sizeof(wifi_ssid));
    wifi_list[i] = strdup(wifi_ssid);
  }
  general_submenu_menu_t menu_aps = {0};
  menu_aps.options = wifi_list;
  menu_aps.options_count = aps_count;
  menu_aps.select_cb = wifi_settings_show_options_list;
  menu_aps.selected_option = 0;
  menu_aps.exit_cb = wifi_settings_exit_app;

  general_submenu(menu_aps);
}

void wifi_settings_begin() {
  wifi_settings_show_list();
}
