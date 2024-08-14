#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  MENU_MAIN_2 = 0,
  MENU_APPLICATIONS_2,
  MENU_SETTINGS_2,
  MENU_ABOUT_2,
  /* Applications */
  MENU_WIFI_APPS_2,
  MENU_BLUETOOTH_APPS_2,
  MENU_ZIGBEE_APPS_2,
  MENU_THREAD_APPS_2,
  MENU_GPS_2,
  /* WiFi applications */
  MENU_WIFI_ANALIZER_2,
  MENU_WIFI_DEAUTH_2,
  MENU_WIFI_DOS_2,
  /* WiFi analizer items */
  MENU_WIFI_ANALYZER_RUN_2,
  MENU_WIFI_ANALYZER_SETTINGS_2,
  MENU_WIFI_ANALYZER_HELP_2,
  /* WiFi analizer start items */
  MENU_WIFI_ANALYZER_ASK_SUMMARY_2,
  MENU_WIFI_ANALYZER_SUMMARY_2,
  /* WiFi analizer settings */
  MENU_WIFI_ANALYZER_CHANNEL_2,
  MENU_WIFI_ANALYZER_DESTINATION_2,
  MENU_WIFI_ANALYZER_SD_EREASE_WARNING_2,
  /* Bluetooth applications */
  MENU_BLUETOOTH_TRAKERS_SCAN_2,
  MENU_BLUETOOTH_SPAM_2,
  /* Zigbee applications */
  MENU_ZIGBEE_SPOOFING_2,
  MENU_ZIGBEE_SWITCH_2,
  MENU_ZIGBEE_LIGHT_2,
  MENU_ZIGBEE_SNIFFER_2,
  /* Thread applications */
  MENU_THREAD_BROADCAST_2,
  MENU_THREAD_SNIFFER_2,
  /* Thread Sniffer App */
  MENU_THREAD_SNIFFER_RUN_2,
  /* GPS applications */
  MENU_GPS_WARDRIVING_2,
  MENU_GPS_DATE_TIME_2,
  MENU_GPS_LOCATION_2,
  MENU_GPS_SPEED_2,
  MENU_GPS_HELP_2,
  /* Wardriving submenus */
  MENU_GPS_WARDRIVING_START_2,
  MENU_GPS_WARDRIVING_HELP_2,
  /* About submenus */
  MENU_ABOUT_VERSION_2,
  MENU_ABOUT_LICENSE_2,
  MENU_ABOUT_CREDITS_2,
  MENU_ABOUT_LEGAL_2,
  MENU_ABOUT_UPDATE_2,
  /* Settings items */
  MENU_SETTINGS_DISPLAY_2,
  MENU_FILE_MANAGER_2,
  MENU_FILE_MANAGER_LOCAL_2,
  MENU_FILE_MANAGER_WEB_2,
  MENU_SETTINGS_SYSTEM_2,
  MENU_SETTINGS_TIME_ZONE_2,
  MENU_SETTINGS_WIFI_2,
  MENU_SETTINGS_SD_CARD_2,
  MENU_SETTINGS_SD_CARD_INFO_2,
  MENU_SETTINGS_SD_CARD_FORMAT_2,
  MENU_STEALTH_MODE_2,
  /* Menu count */
  MENU_COUNT_2,  // Keep this at the end
} menu_idx_t;

typedef struct {
  menu_idx_t menu_idx;
  menu_idx_t parent_idx;
  const char* display_name;
  void* input_cb;
  void* on_enter_cb;
  void* on_exit_cb;
  bool is_visible;
} menu_t;

typedef struct {
  menu_idx_t current_menu;
  uint8_t selected_menu;
  uint8_t submenus_count;
  uint8_t menus_count;
  uint8_t** submenus_idx;
} menus_manager_t;

void apps_exit_cb() {
  printf("apps_exit_cb\n");
}
void apps_enter_cb() {
  printf("apps_enter_cb\n");
}

menu_t menus[] = {{.display_name = "Must Not See This",
                   .menu_idx = MENU_MAIN_2,
                   .parent_idx = -1,
                   .input_cb = NULL,
                   .on_enter_cb = NULL,
                   .on_exit_cb = NULL,
                   .is_visible = false},
                  {.display_name = "MENU_APPLICATIONS",
                   .menu_idx = MENU_APPLICATIONS_2,
                   .parent_idx = MENU_MAIN_2,
                   .input_cb = NULL,
                   .on_enter_cb = apps_enter_cb,
                   .on_exit_cb = apps_exit_cb,
                   .is_visible = true},
                  {.display_name = "MENU_SETTINGS",
                   .menu_idx = MENU_SETTINGS_2,
                   .parent_idx = MENU_MAIN_2,
                   .input_cb = NULL,
                   .on_enter_cb = NULL,
                   .on_exit_cb = NULL,
                   .is_visible = true},
                  {.display_name = "MENU_ABOUT",
                   .menu_idx = MENU_ABOUT_2,
                   .parent_idx = MENU_MAIN_2,
                   .input_cb = NULL,
                   .on_enter_cb = NULL,
                   .on_exit_cb = NULL,
                   .is_visible = true},
                  {.display_name = "MENU_WIFI_APPS",
                   .menu_idx = MENU_WIFI_APPS_2,
                   .parent_idx = MENU_APPLICATIONS_2,
                   .input_cb = NULL,
                   .on_enter_cb = NULL,
                   .on_exit_cb = NULL,
                   .is_visible = true}};

void menus_module_begin();