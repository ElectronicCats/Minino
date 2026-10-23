#include "menus.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "about_module.h"
    #if CONFIG_BT_ENABLED
#include "hid_module.h"
#include "spam_module.h"
#include "trackers_module.h"
#include "adv_scan_module.h"
#include "surv_sim_module.h"
    #endif
#include "analyzer_scenes.h"
#include "captive_module.h"
#include "catdos_module.h"
#include "deauth_module.h"
#include "detector_scenes.h"
#include "display_settings.h"
#include "drone_id_scenes.h"
#include "droneid_scanner.h"
#include "file_manager_module.h"
#include "gps_module.h"
#include "gps_screens.h"
#include "gps_settings.h"
#include "i2c_scanner.h"
#include "logs_output.h"
#include "menu_style.h"
#include "modbus_dos.h"
#include "modbus_tcp_scenes.h"
#include "open_thread_module.h"  // Deprecated since v1.1.14.0
#include "ota_module.h"
#include "sd_card_settings_module.h"
#include "settings_module.h"
#include "sleep_mode_scenes.h"
#include "ssid_spam.h"
#include "stealth_mode.h"
#include "surveillance_module.h"
#include "uart_bridge_app.h"
#include "wardriving_module.h"
#include "wardriving_screens_module.h"
#include "web_file_browser_module.h"
#include "wifi_analyzer.h"
#include "wifi_settings_scenes.h"
#include "hello_module.h"
#include "rf_spectrum.h"
#include "esp_sdr_module.h"

/* Most of the catalogue below is gated on CONFIG_* symbols. In the IDF build
   they arrive through the toolchain, but relying on <stdio.h> to drag
   sdkconfig.h in is fragile: dropping that include would silently compile out
   ~60 entries with no diagnostic. The host test provides its own sdkconfig.h
   on the include path. */
#include "sdkconfig.h"

static menu_t menus[] = {
    //////////////////////////////////
    {.display_name = "Must Not See This",
     .menu_idx = MENU_MAIN,
     .parent_idx = -1,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = false},
    {.display_name = "Applications",
     .menu_idx = MENU_APPLICATIONS,
     .parent_idx = MENU_MAIN,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Settings",
     .menu_idx = MENU_SETTINGS,
     .parent_idx = MENU_MAIN,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "About",
     .menu_idx = MENU_ABOUT,
     .parent_idx = MENU_MAIN,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Version",
     .menu_idx = MENU_ABOUT_VERSION,
     .parent_idx = MENU_ABOUT,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_version,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "License",
     .menu_idx = MENU_ABOUT_LICENSE,
     .parent_idx = MENU_ABOUT,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_license,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Credits",
     .menu_idx = MENU_ABOUT_CREDITS,
     .parent_idx = MENU_ABOUT,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_credits_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Legal",
     .menu_idx = MENU_ABOUT_LEGAL,
     .parent_idx = MENU_ABOUT,
     .last_selected_submenu = 0,
     .on_enter_cb = about_module_display_legal_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
// Uncomment to show the Demo app
// {.display_name = "Hello APP",
//  .menu_idx              = MENU_HELLO,
//  .parent_idx            = MENU_APPLICATIONS,
//  .entry_cmd             = "hello",
//  .last_selected_submenu = 0,
//  .on_enter_cb           = hello_main,
//  .on_exit_cb            = NULL,
//  .is_visible            = true},
    #ifdef CONFIG_WIFI_APPS_ENABLE
    {.display_name = "WiFi",
     .menu_idx = MENU_WIFI_APPS,
     .parent_idx = MENU_APPLICATIONS,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Analyzer",
     .menu_idx = MENU_WIFI_ANALIZER,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "analyzer",
     .last_selected_submenu = 0,
     .on_enter_cb = analyzer_scenes_main_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Captive",
     .menu_idx = MENU_WIFI_CAPTIVE,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "captive",
     .last_selected_submenu = 0,
     .on_enter_cb = captive_module_main,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Deauth",
     .menu_idx = MENU_WIFI_DEAUTH,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "deauth",
     .last_selected_submenu = 0,
     .on_enter_cb = deauth_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Deauth Scan",
     .menu_idx = MENU_WIFI_DEAUTH_SCAN,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "deauth_scan",
     .last_selected_submenu = 0,
     .on_enter_cb = detector_scenes_main_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "DoS",
     .menu_idx = MENU_WIFI_DOS,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "dos",
     .last_selected_submenu = 0,
     .on_enter_cb = catdos_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "SSID Spammer",
     .menu_idx = MENU_WIFI_SSID_SPAM,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "ssid_spam",
     .last_selected_submenu = 0,
     .on_enter_cb = ssid_spam_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
#if defined(CONFIG_BT_ENABLED)
    {.display_name = "Drone ID",
     .menu_idx = MENU_WIFI_ODRONE_ID,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "drone_id",
     .last_selected_submenu = 0,
     .on_enter_cb = drone_id_scenes_main,
     .on_exit_cb = NULL,
     .is_visible = true},
#endif
    #if defined(CONFIG_BT_ENABLED)
    {.display_name = "DroneID Scan",
     .menu_idx = MENU_WIFI_ODRONE_SCANNER,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "droneid_scanner",
     .last_selected_submenu = 0,
     .on_enter_cb = droneid_scanner_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    {.display_name = "Modbus TCP",
     .menu_idx = MENU_WIFI_MBDOS_ID,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "modbus_tcp",
     .last_selected_submenu = 0,
     .on_enter_cb = modbus_tcp_scenes_main,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Flock / ALPR",
     .menu_idx = MENU_WIFI_FLOCK,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "flock",
     .last_selected_submenu = 0,
     .on_enter_cb = surveillance_module_begin_flock,
     .on_exit_cb = surveillance_module_stop,
     .is_visible = true},
    {.display_name = "Flock Sim",
     .menu_idx = MENU_WIFI_FLOCK_SIM,
     .parent_idx = MENU_WIFI_APPS,
     .entry_cmd = "flock_sim",
     .last_selected_submenu = 0,
     .on_enter_cb = surv_sim_begin_flock,
     .on_exit_cb = surv_sim_stop,
     .is_visible = true},
    #endif
    #ifdef CONFIG_BLUETOOTH_APPS_ENABLE
    {.display_name = "Bluetooth",
     .menu_idx = MENU_BLUETOOTH_APPS,
     .parent_idx = MENU_APPLICATIONS,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Spam",
     .menu_idx = MENU_BLUETOOTH_SPAM,
     .parent_idx = MENU_BLUETOOTH_APPS,
     .entry_cmd = "spam",
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Apple",
     .menu_idx = MENU_BLUETOOTH_SPAM_APPLE,
     .parent_idx = MENU_BLUETOOTH_SPAM,
     .entry_cmd = "spam_apple",
     .last_selected_submenu = 0,
     .on_enter_cb = ble_spam_apple_begin,
     .on_exit_cb = ble_spam_stop,
     .is_visible = true},
    {.display_name = "Android",
     .menu_idx = MENU_BLUETOOTH_SPAM_ANDROID,
     .parent_idx = MENU_BLUETOOTH_SPAM,
     .entry_cmd = "spam_android",
     .last_selected_submenu = 0,
     .on_enter_cb = ble_spam_android_begin,
     .on_exit_cb = ble_spam_stop,
     .is_visible = true},
    {.display_name = "Windows",
     .menu_idx = MENU_BLUETOOTH_SPAM_WINDOWS,
     .parent_idx = MENU_BLUETOOTH_SPAM,
     .entry_cmd = "spam_windows",
     .last_selected_submenu = 0,
     .on_enter_cb = ble_spam_windows_begin,
     .on_exit_cb = ble_spam_stop,
     .is_visible = true},
    {.display_name = "Samsung",
     .menu_idx = MENU_BLUETOOTH_SPAM_SAMSUNG,
     .parent_idx = MENU_BLUETOOTH_SPAM,
     .entry_cmd = "spam_samsung",
     .last_selected_submenu = 0,
     .on_enter_cb = ble_spam_samsung_begin,
     .on_exit_cb = ble_spam_stop,
     .is_visible = true},
{.display_name = "All",
      .menu_idx = MENU_BLUETOOTH_SPAM_ALL,
      .parent_idx = MENU_BLUETOOTH_SPAM,
      .entry_cmd = "spam_all",
      .last_selected_submenu = 0,
      .on_enter_cb = ble_spam_all_begin,
      .on_exit_cb = ble_spam_stop,
      .is_visible = true},
     {.display_name = "HID",
     .menu_idx = MENU_BLUETOOTH_HID,
     .parent_idx = MENU_BLUETOOTH_APPS,
     .entry_cmd = "hid",
     .last_selected_submenu = 0,
     .on_enter_cb = hid_module_begin,
     .on_exit_cb = hid_module_stop,
     .is_visible = true},
    {.display_name = "ADV Scanner",
     .menu_idx = MENU_BLUETOOTH_ADV,
     .parent_idx = MENU_BLUETOOTH_APPS,
     .last_selected_submenu = 0,
     .on_enter_cb = adv_scanner_module_begin,
     .on_exit_cb = adv_scanner_module_stop,
     .is_visible = true},
    {.display_name = "Trackers",
     .menu_idx = MENU_BLUETOOTH_TRACKERS_SCAN,
     .parent_idx = MENU_BLUETOOTH_APPS,
     .entry_cmd = "trackers",
     .last_selected_submenu = 0,
     .on_enter_cb = trackers_module_begin,
     .on_exit_cb = trackers_module_on_exit,
     .is_visible = true},
    #endif
    
    #if defined(CONFIG_THREAD_APPS_ENABLE) && defined(CONFIG_MININO_OPENTHREAD_STACK)
    {.display_name = "Thread",
     .menu_idx = MENU_THREAD_APPS,
     .parent_idx = MENU_APPLICATIONS,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Broadcast",
     .menu_idx = MENU_THREAD_BROADCAST,
     .parent_idx = MENU_THREAD_APPS,
     .entry_cmd = "broadcast",
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_broadcast_enter,
     .on_exit_cb = open_thread_module_exit,
     .is_visible = true},
    {.display_name = "Sniffer",
     .menu_idx = MENU_THREAD_SNIFFER,
     .parent_idx = MENU_THREAD_APPS,
     .entry_cmd = "thread_sniffer",
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_sniffer_enter,
     .on_exit_cb = open_thread_module_exit,
     .is_visible = true},
    {.display_name = "Run",
     .menu_idx = MENU_THREAD_SNIFFER_RUN,
     .parent_idx = MENU_THREAD_SNIFFER,
     .last_selected_submenu = 0,
     .on_enter_cb = open_thread_module_sniffer_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #if CONFIG_GPS_APPS_ENABLE && CONFIG_GPS_ENABLED && CONFIG_SD_ENABLED
    {.display_name = "GPS",
     .menu_idx = MENU_GPS,
     .parent_idx = MENU_APPLICATIONS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_check_state,
     .on_exit_cb = gps_module_reset_state,
     .is_visible = true},
    #if CONFIG_WIFI_APPS_ENABLE || CONFIG_ZIGBEE_APPS_ENABLE || \
      CONFIG_THREAD_APPS_ENABLE
    {.display_name = "Wardriving",
     .menu_idx = MENU_GPS_WARDRIVING,
     .parent_idx = MENU_GPS,
     .entry_cmd = "gps_wardriving",
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_module_begin,
     .on_exit_cb = wardriving_module_end,
     .is_visible = true},
    #if CONFIG_WIFI_APPS_ENABLE
    {.display_name = "AP Start",
     .menu_idx = MENU_GPS_WARDRIVING_START,
     .parent_idx = MENU_GPS_WARDRIVING,
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_module_start_scan,
     .on_exit_cb = wardriving_module_stop_scan,
     .is_visible = true},
    #endif
    /* The war bee and war thread apps live in the modules that a build
       variant leaves out when the Zigbee or the Thread stack is off (see
       the filters in main/CMakeLists.txt). The entries have to follow the
       same gate or the link fails on the missing symbols. */
    #if defined(CONFIG_ZIGBEE_APPS_ENABLE) && defined(CONFIG_MININO_ZIGBEE_STACK)
    {.display_name = "Zigbee Start",
     .menu_idx = MENU_GPS_WARDRIVING_BEE_START,
     .parent_idx = MENU_GPS_WARDRIVING,
     .last_selected_submenu = 0,
     .on_enter_cb = warbee_module_begin,
     .on_exit_cb = warbee_module_exit,
     .is_visible = true},
    #endif
    #if defined(CONFIG_THREAD_APPS_ENABLE) && defined(CONFIG_MININO_OPENTHREAD_STACK)
    {.display_name = "Thread Start",
     .menu_idx = MENU_GPS_WARDRIVING_THREAD_START,
     .parent_idx = MENU_GPS_WARDRIVING,
     .last_selected_submenu = 0,
     .on_enter_cb = warthread_module_begin,
     .on_exit_cb = warthread_module_exit,
     .is_visible = true},
    #endif
    {.display_name = "Help",
     .menu_idx = MENU_GPS_WARDRIVING_HELP,
     .parent_idx = MENU_GPS_WARDRIVING,
     .last_selected_submenu = 0,
     .on_enter_cb = wardriving_screens_show_help,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    {.display_name = "Date & Time",
     .menu_idx = MENU_GPS_DATE_TIME,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Location",
     .menu_idx = MENU_GPS_LOCATION,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Speed",
     .menu_idx = MENU_GPS_SPEED,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Route",
     .menu_idx = MENU_GPS_ROUTE,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_general_data_run,
     .on_exit_cb = gps_screens_stop_route_recording,
     .is_visible = true},
    {.display_name = "Num Sats",
     .menu_idx = MENU_GPS_TEST,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_on_test_enter,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Config",
     .menu_idx = MENU_GPS_CONFIG,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_module_on_config_enter,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Help",
     .menu_idx = MENU_GPS_HELP,
     .parent_idx = MENU_GPS,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_screens_show_help,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #ifdef CONFIG_GPIO_APPS_ENABLE
    {.display_name = "GPIO",
     .menu_idx = MENU_GPIO_APPS,
     .parent_idx = MENU_APPLICATIONS,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "I2C Scanner",
     .menu_idx = MENU_GPIO_I2C_SCANNER,
     .parent_idx = MENU_GPIO_APPS,
     .entry_cmd = "i2c_scanner",
     .last_selected_submenu = 0,
     .on_enter_cb = i2c_scanner_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "UART Bridge",
     .menu_idx = MENU_GPIO_UART_BRIDGE,
     .parent_idx = MENU_GPIO_APPS,
     .entry_cmd = "uart_bridge",
     .last_selected_submenu = 0,
     .on_enter_cb = uart_bridge_app_enter,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #ifdef CONFIG_FILE_MANAGER_ENABLE
    {.display_name = "File Manager",
     .menu_idx = MENU_FILE_MANAGER,
     .parent_idx = MENU_APPLICATIONS,
     .entry_cmd = "file_manager",
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    #ifdef CONFIG_FILE_MANAGER_LOCAL
    {.display_name = "Local",
     .menu_idx = MENU_FILE_MANAGER_LOCAL,
     .parent_idx = MENU_FILE_MANAGER,
     .last_selected_submenu = 0,
     .on_enter_cb = file_manager_module_init,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #ifdef CONFIG_FILE_MANAGER_WEB
    {.display_name = "Web",
     .menu_idx = MENU_FILE_MANAGER_WEB,
     .parent_idx = MENU_FILE_MANAGER,
     .last_selected_submenu = 0,
     .on_enter_cb = web_file_browser_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #endif
    #ifdef CONFIG_OTA_ENABLE
    {.display_name = "Update",
     .menu_idx = MENU_ABOUT_UPDATE,
     .parent_idx = MENU_ABOUT,
     .entry_cmd = "ota",
     .last_selected_submenu = 0,
     .on_enter_cb = ota_module_init,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    {.display_name = "Display",
     .menu_idx = MENU_SETTINGS_DISPLAY,
     .parent_idx = MENU_SETTINGS,
     .last_selected_submenu = 0,
     .on_enter_cb = display_config_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "System",
     .menu_idx = MENU_SETTINGS_SYSTEM,
     .parent_idx = MENU_SETTINGS,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Logs Output",
     .menu_idx = MENU_SETTINGS_LOGS_OUTPUT,
     .parent_idx = MENU_SETTINGS_SYSTEM,
     .last_selected_submenu = 0,
     .on_enter_cb = logs_output,
     .on_exit_cb = NULL,
     .is_visible = true},
    #if CONFIG_GPS_APPS_ENABLE && CONFIG_GPS_ENABLED
    {.display_name = "GPS",
     .menu_idx = MENU_SETTINGS_GPS,
     .parent_idx = MENU_SETTINGS_SYSTEM,
     .last_selected_submenu = 0,
     .on_enter_cb = gps_settings_main_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    #if CONFIG_SD_ENABLED
    {.display_name = "SD card",
     .menu_idx = MENU_SETTINGS_SD_CARD,
     .parent_idx = MENU_SETTINGS_SYSTEM,
     .last_selected_submenu = 0,
     .on_enter_cb = NULL,
     .on_exit_cb = NULL,
     .is_visible = true},
    /* The three entries below are children of MENU_SETTINGS_SD_CARD, so they
       have to be compiled under the same gate: outside it they become orphans
       and navigation_exit() lands on a parent that does not resolve. */
    {.display_name = "Info",
     .menu_idx = MENU_SETTINGS_SD_CARD_INFO,
     .parent_idx = MENU_SETTINGS_SD_CARD,
     .last_selected_submenu = 0,
     .on_enter_cb = update_sd_card_info,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Check Format",
     .menu_idx = MENU_SETTINGS_SD_CARD_CHECK_FORMAT,
     .parent_idx = MENU_SETTINGS_SD_CARD,
     .last_selected_submenu = 0,
     .on_enter_cb = sd_card_settings_verify_sd_card,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Format",
     .menu_idx = MENU_SETTINGS_SD_CARD_FORMAT,
     .parent_idx = MENU_SETTINGS_SD_CARD,
     .last_selected_submenu = 0,
     .on_enter_cb = sd_card_settings_verify_sd_card,
     .on_exit_cb = NULL,
     .is_visible = true},
    #endif
    {.display_name = "WiFi",
     .menu_idx = MENU_SETTINGS_WIFI,
     .parent_idx = MENU_SETTINGS_SYSTEM,
     .last_selected_submenu = 0,
     .on_enter_cb = wifi_settings_scenes_main,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Stealth Mode",
     .menu_idx = MENU_STEALTH_MODE,
     .parent_idx = MENU_SETTINGS,
     .last_selected_submenu = 0,
     .on_enter_cb = stealth_mode_open_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Menu Style",
     .menu_idx = MENU_SETTINGS_MENU_STYLE,
     .parent_idx = MENU_SETTINGS,
     .last_selected_submenu = 0,
     .on_enter_cb = menu_style_open_menu,
     .on_exit_cb = NULL,
     .is_visible = true},
    {.display_name = "Sleep Mode",
     .menu_idx = MENU_SLEEP_MODE,
     .parent_idx = MENU_SETTINGS,
     .last_selected_submenu = 0,
     .on_enter_cb = sleep_mode_scenes_main,
     .on_exit_cb = NULL,
     .is_visible = true},
    // {.display_name = "ZB CLI",
    //  .menu_idx = MENU_ZB_CLI,
    //  .parent_idx = MENU_SETTINGS,
    //  .last_selected_submenu = 0,
    //  .on_enter_cb = zbcli_settings_main,
    //  .on_exit_cb = NULL,
    //  .is_visible = true},
    #if defined(CONFIG_SDR_ENABLE)
    {.display_name = "SDR",
     .menu_idx = MENU_ESP_SDR,
     .parent_idx = MENU_APPLICATIONS,
     .entry_cmd = "sdr",
     .last_selected_submenu = 0,
     .on_enter_cb = esp_sdr_module_begin,
     .on_exit_cb = esp_sdr_module_stop,
     .is_visible = true},
    #endif
    {.display_name = "Spectrum",
     .menu_idx = MENU_SPECTRUM,
     .parent_idx = MENU_APPLICATIONS,
     .entry_cmd = "spectrum",
     .last_selected_submenu = 0,
     .on_enter_cb = rf_spectrum_app_begin,
     .on_exit_cb = rf_spectrum_app_stop,
     .is_visible = true}
};

menu_t* menus_get(void) {
  return menus;
}

uint8_t menus_count(void) {
  return sizeof(menus) / sizeof(menus[0]);
}

/* menus_count() returns uint8_t, so anything at or past 256 entries would be
   silently truncated (e.g. 260 -> 4) and most of the tree would become
   unreachable. MENU_COUNT is the bound menus_module.c sizes its submenu
   arrays by. */
_Static_assert(sizeof(menus) / sizeof(menus[0]) <= MENU_COUNT,
               "menu catalogue has more entries than MENU_COUNT");
_Static_assert(sizeof(menus) / sizeof(menus[0]) < 256,
               "menus_count() truncates at 256 entries");

int menus_find_index(menu_idx_t idx) {
  for (uint8_t i = 0; i < menus_count(); i++) {
    if (menus[i].menu_idx == idx) {
      return (int) i;
    }
  }
  return -1;
}

menu_t* menus_find(menu_idx_t idx) {
  int pos = menus_find_index(idx);
  if (pos < 0) return NULL;
  return &menus[pos];
}
