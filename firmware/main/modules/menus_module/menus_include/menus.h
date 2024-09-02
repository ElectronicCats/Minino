#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "about_module.h"
#include "hid_module.h"
#include "spam_module.h"
#include "trackers_module.h"
#include "z_switch_module.h"

#include "catdos_module.h"
#include "deauth_module.h"
#include "display_settings.h"
#include "file_manager_module.h"
#include "gps_module.h"
#include "gps_screens.h"
#include "open_thread_module.h"
#include "ota_module.h"
#include "sd_card_settings_module.h"
#include "settings_module.h"
#include "stealth_mode.h"
#include "wardriving_module.h"
#include "wardriving_screens_module.h"
#include "web_file_browser_module.h"
#include "wifi_module.h"
#include "wifi_settings.h"
#include "zigbee_module.h"

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
  MENU_BLUETOOTH_HID,
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
  uint8_t last_selected_submenu;
  void* on_enter_cb;
  void* on_exit_cb;
  bool is_visible;
} menu_t;

typedef struct {
  menu_idx_t current_menu;
  menu_idx_t parent_menu_idx;
  uint8_t selected_submenu;
  uint8_t submenus_count;
  uint8_t menus_count;
  uint8_t** submenus_idx;
  bool input_lock;
} menus_manager_t;

menu_t menus[] = {  //////////////////////////////////
    {.display_name = "Must Not See This",
     .menu_idx = MENU_MAIN_2,
     .parent_idx = -1,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = false},
    {.display_name = "Applications",
     .menu_idx = MENU_APPLICATIONS_2,
     .parent_idx = MENU_MAIN_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Settings",
     .menu_idx = MENU_SETTINGS_2,
     .parent_idx = MENU_MAIN_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "About",
     .menu_idx = MENU_ABOUT_2,
     .parent_idx = MENU_MAIN_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Version",
     .menu_idx = MENU_ABOUT_VERSION_2,
     .parent_idx = MENU_ABOUT_2,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_version,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "License",
     .menu_idx = MENU_ABOUT_LICENSE_2,
     .parent_idx = MENU_ABOUT_2,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_license,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Credits",
     .menu_idx = MENU_ABOUT_CREDITS_2,
     .parent_idx = MENU_ABOUT_2,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_credits_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Legal",
     .menu_idx = MENU_ABOUT_LEGAL_2,
     .parent_idx = MENU_ABOUT_2,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_legal_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
#ifdef CONFIG_WIFI_APPS_ENABLE
    {.display_name = "WiFi",
     .menu_idx = MENU_WIFI_APPS_2,
     .parent_idx = MENU_APPLICATIONS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_WIFI_APP_ANALYZER
    {.display_name = "Analyzer",
     .menu_idx = MENU_WIFI_ANALIZER_2,
     .parent_idx = MENU_WIFI_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_module_analizer_begin,
     .on_exit_cb = wifi_module_analyzer_exit,
     .is_visible = true},
    {.display_name = "Start",
     .menu_idx = MENU_WIFI_ANALYZER_RUN_2,
     .parent_idx = MENU_WIFI_ANALIZER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_module_analyzer_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Settings",
     .menu_idx = MENU_WIFI_ANALYZER_SETTINGS_2,
     .parent_idx = MENU_WIFI_ANALIZER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Channel",
     .menu_idx = MENU_WIFI_ANALYZER_CHANNEL_2,
     .parent_idx = MENU_WIFI_ANALYZER_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_module_analyzer_channel,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Destination",
     .menu_idx = MENU_WIFI_ANALYZER_DESTINATION_2,
     .parent_idx = MENU_WIFI_ANALYZER_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_module_analyzer_destination,
     .on_exit_cb = wifi_module_analyzer_destination_exit,
     .is_visible = true},
    {.display_name = "Help",
     .menu_idx = MENU_WIFI_ANALYZER_HELP_2,
     .parent_idx = MENU_WIFI_ANALIZER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_module_show_analyzer_help,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_WIFI_APP_DEAUTH
    {.display_name = "Deauth",
     .menu_idx = MENU_WIFI_DEAUTH_2,
     .parent_idx = MENU_WIFI_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = deauth_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_WIFI_APP_DOS
    {.display_name = "DoS",
     .menu_idx = MENU_WIFI_DOS_2,
     .parent_idx = MENU_WIFI_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = catdos_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
#endif
#ifdef CONFIG_BLUETOOTH_APPS_ENABLE
    {.display_name = "Bluetooth",
     .menu_idx = MENU_BLUETOOTH_APPS_2,
     .parent_idx = MENU_APPLICATIONS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_BLUETOOTH_APP_TRAKERS
    {.display_name = "Trakers scan",
     .menu_idx = MENU_BLUETOOTH_TRAKERS_SCAN_2,
     .parent_idx = MENU_BLUETOOTH_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = trackers_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_BLUETOOTH_APP_SPAM
    {.display_name = "Spam",
     .menu_idx = MENU_BLUETOOTH_SPAM_2,
     .parent_idx = MENU_BLUETOOTH_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = ble_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_BLUETOOTH_APP_HID
    {.display_name = "HID",
     .menu_idx = MENU_BLUETOOTH_HID,
     .parent_idx = MENU_BLUETOOTH_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = hid_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
#endif
#ifdef CONFIG_ZIGBEE_APPS_ENABLE
    {.display_name = "Zigbee",
     .menu_idx = MENU_ZIGBEE_APPS_2,
     .parent_idx = MENU_APPLICATIONS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_ZIGBEE_APP_SPOOFING
    {.display_name = "Spoofing",
     .menu_idx = MENU_ZIGBEE_SPOOFING_2,
     .parent_idx = MENU_ZIGBEE_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Switch",
     .menu_idx = MENU_ZIGBEE_SWITCH_2,
     .parent_idx = MENU_ZIGBEE_SPOOFING_2,
     .last_selected_submenu = 0,
     .on_enter_cb = zigbee_module_switch_enter,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Light",
     .menu_idx = MENU_ZIGBEE_LIGHT_2,
     .parent_idx = MENU_ZIGBEE_SPOOFING_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_ZIGBEE_APP_SNIFFER
    {.display_name = "Sniffer",
     .menu_idx = MENU_ZIGBEE_SNIFFER_2,
     .parent_idx = MENU_ZIGBEE_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = zigbee_module_sniffer_enter,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
#endif
#ifdef CONFIG_THREAD_APPS_ENABLE
    {.display_name = "Thread",
     .menu_idx = MENU_THREAD_APPS_2,
     .parent_idx = MENU_APPLICATIONS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_THREAD_APP_BROADCAST
    {.display_name = "Broadcast",
     .menu_idx = MENU_THREAD_BROADCAST_2,
     .parent_idx = MENU_THREAD_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_broadcast_enter,
     .on_exit_cb = open_thread_module_exit,
     .is_visible = true},
  #endif
  #ifdef CONFIG_THREAD_APP_SNIFFER
    {.display_name = "Sniffer",
     .menu_idx = MENU_THREAD_SNIFFER_2,
     .parent_idx = MENU_THREAD_APPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_sniffer_enter,
     .on_exit_cb = open_thread_module_exit,
     .is_visible = true},
    {.display_name = "Run",
     .menu_idx = MENU_THREAD_SNIFFER_RUN_2,
     .parent_idx = MENU_THREAD_SNIFFER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_sniffer_run,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
#endif
#ifdef CONFIG_GPS_APPS_ENABLE
    {.display_name = "GPS",
     .menu_idx = MENU_GPS_2,
     .parent_idx = MENU_APPLICATIONS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_GPS_APP_WARDRIVING
    {.display_name = "Wardriving",
     .menu_idx = MENU_GPS_WARDRIVING_2,
     .parent_idx = MENU_GPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_module_begin,
     .on_exit_cb = wardriving_module_end,
     .is_visible = true},
    {.display_name = "Start",
     .menu_idx = MENU_GPS_WARDRIVING_START_2,
     .parent_idx = MENU_GPS_WARDRIVING_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_module_start_scan,
     .on_exit_cb = wardriving_module_stop_scan,
     .is_visible = true},
    {.display_name = "Help",
     .menu_idx = MENU_GPS_WARDRIVING_HELP_2,
     .parent_idx = MENU_GPS_WARDRIVING_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_screens_show_help,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
    {.display_name = "Date & Time",
     .menu_idx = MENU_GPS_DATE_TIME_2,
     .parent_idx = MENU_GPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Location",
     .menu_idx = MENU_GPS_LOCATION_2,
     .parent_idx = MENU_GPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Speed",
     .menu_idx = MENU_GPS_SPEED_2,
     .parent_idx = MENU_GPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Help",
     .menu_idx = MENU_GPS_HELP_2,
     .parent_idx = MENU_GPS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_screens_show_help,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Time zone",
     .menu_idx = MENU_SETTINGS_TIME_ZONE_2,
     .parent_idx = MENU_SETTINGS_SYSTEM_2,
     .last_selected_submenu = 0,
     .on_enter_cb = settings_module_time_zone,
     .on_exit_cb = NULL,
     .is_visible = true},
#endif
#ifdef CONFIG_OTA_ENABLE
    {.display_name = "Update",
     .menu_idx = MENU_ABOUT_UPDATE_2,
     .parent_idx = MENU_ABOUT_2,
     .last_selected_submenu = 0,
     .on_enter_cb = ota_module_init,
     .on_exit_cb = NULL,
     .is_visible = true},
#endif
    {.display_name = "Display",
     .menu_idx = MENU_SETTINGS_DISPLAY_2,
     .parent_idx = MENU_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = display_config_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "System",
     .menu_idx = MENU_SETTINGS_SYSTEM_2,
     .parent_idx = MENU_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "WiFi",
     .menu_idx = MENU_SETTINGS_WIFI_2,
     .parent_idx = MENU_SETTINGS_SYSTEM_2,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_settings_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "SD card",
     .menu_idx = MENU_SETTINGS_SD_CARD_2,
     .parent_idx = MENU_SETTINGS_SYSTEM_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Info",
     .menu_idx = MENU_SETTINGS_SD_CARD_INFO_2,
     .parent_idx = MENU_SETTINGS_SD_CARD_2,
     .last_selected_submenu = 0,
     .on_enter_cb = update_sd_card_info,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Check Format",
     .menu_idx = MENU_SETTINGS_SD_CARD_FORMAT_2,
     .parent_idx = MENU_SETTINGS_SD_CARD_2,
     .last_selected_submenu = 0,
     .on_enter_cb = sd_card_settings_verify_sd_card,
     .on_exit_cb = NULL,
     .is_visible = true},
#ifdef CONFIG_FILE_MANAGER_ENABLE
    {.display_name = "File Manager",
     .menu_idx = MENU_FILE_MANAGER_2,
     .parent_idx = MENU_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
  #ifdef CONFIG_FILE_MANAGER_LOCAL
    {.display_name = "Local",
     .menu_idx = MENU_FILE_MANAGER_LOCAL_2,
     .parent_idx = MENU_FILE_MANAGER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = file_manager_module_init,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
  #ifdef CONFIG_FILE_MANAGER_WEB
    {.display_name = "Web",
     .menu_idx = MENU_FILE_MANAGER_WEB_2,
     .parent_idx = MENU_FILE_MANAGER_2,
     .last_selected_submenu = 0,
     .on_enter_cb = web_file_browser_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
  #endif
#endif
    {.display_name = "Stealth Mode",
     .menu_idx = MENU_STEALTH_MODE_2,
     .parent_idx = MENU_SETTINGS_2,
     .last_selected_submenu = 0,
     .on_enter_cb = stealth_mode_open_menu,
     .on_exit_cb = NULL,
     .is_visible = true}};