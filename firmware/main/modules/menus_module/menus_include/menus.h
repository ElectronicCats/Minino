#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Menu tree types and catalogue.

   The catalogue itself (menu_t menus[]) lives in menus_data.c and is reached
   through menus_get()/menus_count(); it used to be *defined* here, which gave
   every translation unit including menus_module.h its own copy of it. */

typedef enum {
  MENU_MAIN = 0,
  MENU_APPLICATIONS,
  MENU_SETTINGS,
  MENU_ABOUT,
  /* Applications */
  MENU_WIFI_APPS,
  MENU_BLUETOOTH_APPS,
  MENU_ZIGBEE_APPS,  // Deprecated since v1.1.14.0
  MENU_THREAD_APPS,
  MENU_GPS,
  MENU_GPIO_APPS,
  MENU_WIFI_FLOCK,
  MENU_WIFI_FLOCK_SIM,
  // Uncomment to add the minino demo to menu
  // MENU_HELLO,
  /* WiFi applications */
  MENU_WIFI_ANALIZER,
  MENU_WIFI_CAPTIVE,
  MENU_WIFI_DEAUTH,
  MENU_WIFI_DEAUTH_SCAN,
  MENU_WIFI_DOS,
  MENU_WIFI_SSID_SPAM,
  MENU_WIFI_ODRONE_ID,
  MENU_WIFI_ODRONE_SCANNER,
  MENU_WIFI_MBDOS_ID,
  /* WiFi analizer items */
  MENU_WIFI_ANALYZER_RUN,
  MENU_WIFI_ANALYZER_SETTINGS,
  MENU_WIFI_ANALYZER_HELP,
  /* WiFi analizer start items */
  MENU_WIFI_ANALYZER_ASK_SUMMARY,
  MENU_WIFI_ANALYZER_SUMMARY,
  /* WiFi analizer settings */
  MENU_WIFI_ANALYZER_CHANNEL,
  MENU_WIFI_ANALYZER_DESTINATION,
  MENU_WIFI_ANALYZER_SD_EREASE_WARNING,
  /* Bluetooth applications */
  MENU_BLUETOOTH_TRACKERS_SCAN,
  MENU_BLUETOOTH_SPAM,
  MENU_BLUETOOTH_SPAM_APPLE,
  MENU_BLUETOOTH_SPAM_ANDROID,
  MENU_BLUETOOTH_SPAM_WINDOWS,
  MENU_BLUETOOTH_SPAM_SAMSUNG,
  MENU_BLUETOOTH_SPAM_ALL,
  MENU_BLUETOOTH_HID,
  MENU_BLUETOOTH_ADV,
  /* Zigbee applications */
  MENU_ZIGBEE_SPOOFING,
  MENU_ZIGBEE_SWITCH,
  MENU_ZIGBEE_LIGHT,
  MENU_ZIGBEE_SNIFFER,
  /* Thread applications */
  MENU_THREAD_BROADCAST,
  MENU_THREAD_SNIFFER,
  MENU_THREAD_SNIFFER_RUN,
  /* GPS applications */
  MENU_GPS_WARDRIVING,
  MENU_GPS_DATE_TIME,
  MENU_GPS_LOCATION,
  MENU_GPS_SPEED,
  MENU_GPS_ROUTE,
  MENU_GPS_TEST,
  MENU_GPS_CONFIG,
  MENU_GPS_HELP,
  /* GPIO applications */
  MENU_GPIO_I2C_SCANNER,
  MENU_GPIO_UART_BRIDGE,
  MENU_GPIO_UART_BRIDGE_START,
  MENU_GPIO_UART_BRIDGE_BAUDRATE,
  MENU_GPIO_UART_BRIDGE_DATA_BITS,
  MENU_GPIO_UART_BRIDGE_PARITY,
  MENU_GPIO_UART_BRIDGE_STOP_BITS,
  /* Wardriving submenus */
  MENU_GPS_WARDRIVING_START,
  MENU_GPS_WARDRIVING_BEE_START,     // Deprecated since v1.1.14.0
  MENU_GPS_WARDRIVING_THREAD_START,  // Deprecated since v1.1.14.0
  MENU_GPS_WARDRIVING_HELP,
  /* About submenus */
  MENU_ABOUT_VERSION,
  MENU_ABOUT_LICENSE,
  MENU_ABOUT_CREDITS,
  MENU_ABOUT_LEGAL,
  MENU_ABOUT_UPDATE,
  /* Settings items */
  MENU_SETTINGS_DISPLAY,
  MENU_FILE_MANAGER,
  MENU_FILE_MANAGER_LOCAL,
  MENU_FILE_MANAGER_WEB,
  MENU_SETTINGS_SYSTEM,
  MENU_SETTINGS_LOGS_OUTPUT,
  MENU_SETTINGS_GPS,
  MENU_SETTINGS_WIFI,
  MENU_SETTINGS_SD_CARD,
  MENU_SETTINGS_SD_CARD_INFO,
  MENU_SETTINGS_SD_CARD_CHECK_FORMAT,
  MENU_SETTINGS_SD_CARD_FORMAT,
  MENU_STEALTH_MODE,
  MENU_SLEEP_MODE,
  MENU_SETTINGS_MENU_STYLE,
  MENU_ESP_SDR,
  MENU_SPECTRUM,
  // MENU_ZB_CLI, // Deprecated since v1.1.14.0
  /* Menu count */
  MENU_COUNT,  // Keep this at the end
} menu_idx_t;

/* Callback invoked when a menu is entered/exited. */
typedef void (*menu_cb_t)(void);

typedef struct {
  menu_idx_t menu_idx;
  menu_idx_t parent_idx;
  const char* display_name;
  const char* entry_cmd;
  uint8_t last_selected_submenu;
  menu_cb_t on_enter_cb;
  menu_cb_t on_exit_cb;
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

/* Mutable menu catalogue, in declaration order. Callers must not assume the
   array index matches a menu_idx_t value: entries are compiled in and out by
   CONFIG_* flags, so always resolve ids with menus_find_index(). */
menu_t* menus_get(void);

/* Number of entries in the catalogue. */
uint8_t menus_count(void);

/* Catalogue index of `id`, or -1 when the entry is not compiled in. */
int menus_find_index(menu_idx_t id);

/* Catalogue entry for `id`, or NULL when it is not compiled in. */
menu_t* menus_find(menu_idx_t id);
