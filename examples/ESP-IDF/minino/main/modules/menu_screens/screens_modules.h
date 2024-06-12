#pragma once

#include <stddef.h>

/**
 * @brief Scrolling text flag
 *
 * Used to identify if the text is scrollable
 *
 * Add this flag to the text to make it scrollable
 */
#define VERTICAL_SCROLL_TEXT "vertical_scroll_text"

/**
 * @brief Configuration menu items
 *
 * Used to identify the configuration menu items
 *
 * Add this flag to the menu item to make it a configuration item
 */
#define CONFIGURATION_MENU_ITEMS "configuration"

#define QUESTION_MENU_ITEMS "question"

/**
 * @brief Enum of menus
 *
 * Used to navigate through the different menus
 *
 * Modify this menu also requires to modify the `menu_list`, `next_menu_table`,
 * `prev_menu_table` and `menu_items` arrays
 */
typedef enum {
  MENU_MAIN = 0,
  MENU_APPLICATIONS,
  MENU_SETTINGS,
  MENU_ABOUT,
  /* Applications */
  MENU_WIFI_APPS,
  MENU_BLUETOOTH_APPS,
  MENU_ZIGBEE_APPS,
  MENU_THREAD_APPS,
  MENU_MATTER_APPS,
  MENU_GPS,
  /* WiFi applications */
  MENU_WIFI_ANALIZER,
  MENU_WIFI_DEAUTH,
  /* WiFi analizer items */
  MENU_WIFI_ANALIZER_RUN,
  MENU_WIFI_ANALIZER_SETTINGS,
  /* WiFi analizer start items */
  MENU_WIFI_ANALIZER_ASK_SUMMARY,
  MENU_WIFI_ANALIZER_SUMMARY,
  /* WiFi analizer settings */
  MENU_WIFI_ANALIZER_CHANNEL,
  MENU_WIFI_ANALIZER_DESTINATION,
  /* Bluetooth applications */
  MENU_BLUETOOTH_TRAKERS_SCAN,
  MENU_BLUETOOTH_SPAM,
  /* Zigbee applications */
  MENU_ZIGBEE_SPOOFING,
  MENU_ZIGBEE_SWITCH,
  MENU_ZIGBEE_LIGHT,
  MENU_ZIGBEE_SNIFFER,
  /* Thread applications */
  MENU_THREAD_BROADCAST,
  /* GPS applications */
  MENU_GPS_DATE_TIME,
  MENU_GPS_LOCATION,
  /* About items */
  MENU_ABOUT_VERSION,
  MENU_ABOUT_LICENSE,
  MENU_ABOUT_CREDITS,
  MENU_ABOUT_LEGAL,
  /* Settings items */
  MENU_SETTINGS_DISPLAY,
  MENU_SETTINGS_SOUND,
  MENU_SETTINGS_SYSTEM,
  /* About submenus */
  /* Menu count */
  MENU_COUNT,
} screen_module_menu_t;

/**
 * @brief List of menus
 *
 * Used to get the menu name from the enum value
 * following the order of the `screen_module_menu_t` enum
 *
 * Usage: menu_list[screen_module_menu_t]
 */
const char* menu_list[] = {
    "MENU_MAIN",
    "MENU_APPLICATIONS",
    "MENU_SETTINGS",
    "MENU_ABOUT",
    "MENU_WIFI_APPS",
    "MENU_BLUETOOTH_APPS",
    "MENU_ZIGBEE_APPS",
    "MENU_THREAD_APPS",
    "MENU_MATTER_APPS",
    "MENU_GPS",
    "MENU_WIFI_ANALIZER",
    "MENU_WIFI_DEAUTH",
    "MENU_WIFI_ANALIZER_RUN",
    "MENU_WIFI_ANALIZER_SETTINGS",
    "MENU_WIFI_ANALIZER_ASK_SUMMARY",
    "MENU_WIFI_ANALIZER_SUMMARY",
    "MENU_WIFI_ANALIZER_CHANNEL",
    "MENU_WIFI_ANALIZER_DESTINATION",
    "MENU_BLUETOOTH_TRAKERS_SCAN",
    "MENU_BLUETOOTH_SPAM",
    "MENU_ZIGBEE_SPOOFING",
    "MENU_ZIGBEE_SWITCH",
    "MENU_ZIGBEE_LIGHT",
    "MENU_ZIGBEE_SNIFFER",
    "MENU_THREAD_BROADCAST",
    "MENU_GPS_DATE_TIME",
    "MENU_GPS_LOCATION",
    "MENU_ABOUT_VERSION",
    "MENU_ABOUT_LICENSE",
    "MENU_ABOUT_CREDITS",
    "MENU_ABOUT_LEGAL",
    "MENU_SETTINGS_DISPLAY",
    "MENU_SETTINGS_SOUND",
    "MENU_SETTINGS_SYSTEM",
};

/**
 * @brief List of menus
 *
 * Used to get the next menu to display when the user selects an option
 * following the order of the `screen_module_menu_t` enum
 *
 * Usage: next_menu_table[screen_module_menu_t][selected_item]
 */
const int next_menu_table[][6] = {
    // MENU_MAIN
    {MENU_APPLICATIONS, MENU_SETTINGS, MENU_ABOUT},
    // MENU_APPLICATIONS
    {MENU_WIFI_APPS, MENU_BLUETOOTH_APPS, MENU_ZIGBEE_APPS, MENU_THREAD_APPS,
     MENU_MATTER_APPS, MENU_GPS},
    // MENU_SETTINGS
    {MENU_SETTINGS_DISPLAY, MENU_SETTINGS_SOUND, MENU_SETTINGS_SYSTEM},
    // MENU_ABOUT
    {MENU_ABOUT_VERSION, MENU_ABOUT_LICENSE, MENU_ABOUT_CREDITS,
     MENU_ABOUT_LEGAL},
    // MENU_WIFI_APPS
    {MENU_WIFI_ANALIZER, MENU_WIFI_DEAUTH},
    // MENU_BLUETOOTH_APPS
    {MENU_BLUETOOTH_TRAKERS_SCAN, MENU_BLUETOOTH_SPAM},
    // MENU_ZIGBEE_APPS
    {MENU_ZIGBEE_SPOOFING, MENU_ZIGBEE_SNIFFER},
    // MENU_THREAD_APPS
    {MENU_THREAD_BROADCAST},
    // MENU_MATTER_APPS
    {MENU_MATTER_APPS},
    // MENU_GPS
    {MENU_GPS_DATE_TIME, MENU_GPS_LOCATION},
    // MENU_WIFI_ANALIZER
    {MENU_WIFI_ANALIZER_RUN, MENU_WIFI_ANALIZER_SETTINGS},
    // MENU_WIFI_DEAUTH
    {MENU_WIFI_DEAUTH},
    // MENU_WIFI_ANALIZER_RUN
    {MENU_WIFI_ANALIZER_RUN},
    // MENU_WIFI_ANALIZER_SETTINGS
    {MENU_WIFI_ANALIZER_CHANNEL, MENU_WIFI_ANALIZER_DESTINATION},
    // MENU_WIFI_ANALIZER_ASK_SUMMARY [0] -> Yes, [1] -> No
    {MENU_WIFI_ANALIZER_SUMMARY, MENU_WIFI_ANALIZER},
    // MENU_WIFI_ANALIZER_SUMMARY
    {MENU_WIFI_ANALIZER_SUMMARY},
    // MENU_WIFI_ANALIZER_CHANNEL
    {MENU_WIFI_ANALIZER_CHANNEL},
    // MENU_WIFI_ANALIZER_DESTINATION
    {MENU_WIFI_ANALIZER_DESTINATION},
    // MENU_BLUETOOTH_TRAKERS_SCAN
    {MENU_BLUETOOTH_TRAKERS_SCAN},
    // MENU_BLUETOOTH_SPAM
    {MENU_BLUETOOTH_SPAM},
    // MENU_ZIGBEE_SPOOFING
    {MENU_ZIGBEE_SWITCH, MENU_ZIGBEE_LIGHT},
    // MENU_ZIGBEE_SWITCH
    {MENU_ZIGBEE_SWITCH},
    // MENU_ZIGBEE_LIGHT
    {MENU_ZIGBEE_LIGHT},
    // MENU_ZIGBEE_SNIFFER
    {MENU_ZIGBEE_SNIFFER},
    // MENU_THREAD_BROADCAST
    {MENU_THREAD_BROADCAST},
    // MENU_GPS_DATE_TIME
    {MENU_GPS_DATE_TIME},
    // MENU_GPS_LOCATION
    {MENU_GPS_LOCATION},
    // MENU_ABOUT_VERSION
    {MENU_ABOUT_VERSION},
    // MENU_ABOUT_LICENSE
    {MENU_ABOUT_LICENSE},
    // MENU_ABOUT_CREDITS
    {MENU_ABOUT_CREDITS},
    // MENU_ABOUT_LEGAL
    {MENU_ABOUT_LEGAL},
    // MENU_SETTINGS_DISPLAY
    {MENU_SETTINGS_DISPLAY},
    // MENU_SETTINGS_SOUND
    {MENU_SETTINGS_SOUND},
    // MENU_SETTINGS_SYSTEM
    {MENU_SETTINGS_SYSTEM},
};

/**
 * @brief List of menus
 *
 * Used to get the previous menu to display when the user returns to the
 * previous menu in `menu_screens_exit_submenu`. Add the previous menu
 * following the order of the `screen_module_menu_t` enum
 *
 * Usage: prev_menu_table[screen_module_menu_t]
 */
const int prev_menu_table[] = {
    MENU_MAIN,                       // MENU_MAIN
    MENU_MAIN,                       // MENU_APPLICATIONS
    MENU_MAIN,                       // MENU_SETTINGS
    MENU_MAIN,                       // MENU_ABOUT
    MENU_APPLICATIONS,               // MENU_WIFI_APPS
    MENU_APPLICATIONS,               // MENU_BLUETOOTH_APPS
    MENU_APPLICATIONS,               // MENU_ZIGBEE_APPS
    MENU_APPLICATIONS,               // MENU_THREAD_APPS
    MENU_APPLICATIONS,               // MENU_MATTER_APPS
    MENU_APPLICATIONS,               // MENU_GPS
    MENU_WIFI_APPS,                  // MENU_WIFI_ANALIZER
    MENU_WIFI_APPS,                  // MENU_WIFI_DEAUTH
    MENU_WIFI_ANALIZER_ASK_SUMMARY,  // MENU_WIFI_ANALIZER_RUN
    MENU_WIFI_ANALIZER,              // MENU_WIFI_ANALIZER_SETTINGS
    MENU_WIFI_ANALIZER_RUN,          // MENU_WIFI_ANALIZER_ASK_SUMMARY
    MENU_WIFI_ANALIZER,              // MENU_WIFI_ANALIZER_SUMMARY
    MENU_WIFI_ANALIZER_SETTINGS,     // MENU_WIFI_ANALIZER_CHANNEL
    MENU_WIFI_ANALIZER_SETTINGS,     // MENU_WIFI_ANALIZER_DESTINATION
    MENU_BLUETOOTH_APPS,             // MENU_BLUETOOTH_TRAKERS_SCAN
    MENU_BLUETOOTH_APPS,             // MENU_BLUETOOTH_SPAM
    MENU_ZIGBEE_APPS,                // MENU_ZIGBEE_SPOOFING
    MENU_ZIGBEE_SPOOFING,            // MENU_ZIGBEE_SWITCH
    MENU_ZIGBEE_SPOOFING,            // MENU_ZIGBEE_LIGHT
    MENU_ZIGBEE_APPS,                // MENU_ZIGBEE_SNIFFER
    MENU_THREAD_APPS,                // MENU_THREAD_BROADCAST
    MENU_GPS,                        // MENU_GPS_DATE_TIME
    MENU_GPS,                        // MENU_GPS_LOCATION
    MENU_ABOUT,                      // MENU_ABOUT_VERSION
    MENU_ABOUT,                      // MENU_ABOUT_LICENSE
    MENU_ABOUT,                      // MENU_ABOUT_CREDITS
    MENU_ABOUT,                      // MENU_ABOUT_LEGAL
    MENU_SETTINGS,                   // MENU_SETTINGS_DISPLAY
    MENU_SETTINGS,                   // MENU_SETTINGS_SOUND
    MENU_SETTINGS,                   // MENU_SETTINGS_SYSTEM
};

/**
 * @brief History of selected items in each menu
 *
 * Used to keep track of the selected item in each menu
 *
 * Usage: selected_item_history[screen_module_menu_t]
 */
int selected_item_history[MENU_COUNT] = {0};

char* main_items[] = {
    "Applications",
    "Settings",
    "About",
    NULL,
};

char* applications_items[] = {
    "WiFi", "Bluetooth", "Zigbee", "Thread", "Matter", "GPS", NULL,
};

char* settings_items[] = {
    "Display",
    "Sound",
    "System",
    NULL,
};

char* about_items[] = {
    "Version", "License", "Credits", "Legal", NULL,
};

char* version_text[] = {
    VERTICAL_SCROLL_TEXT,
    /***************/
    "",
    "",
    "",
    " Minino v1.3.0",
    "     BETA",
    NULL,
};

char* license_text[] = {
    VERTICAL_SCROLL_TEXT,
    /***************/
    "",
    "",
    "",
    "  GNU GPL 3.0",
    NULL,
};

char* credits_text[] = {
    VERTICAL_SCROLL_TEXT,
    /***************/
    "Developed by",
    "Electronic Cats",
    "",
    "This product is",
    "in a BETA stage",
    "use at your own",
    NULL,
};

char* legal_text[] = {
    VERTICAL_SCROLL_TEXT,
    /***************/
    "The user",
    "assumes all",
    "responsibility",
    "for the use of",
    "MININO and",
    "agrees to use",
    "it legally and",
    "ethically,",
    "avoiding any",
    "activities that",
    "may cause harm,",
    "interference,",
    "or unauthorized",
    "access to",
    "systems or data.",
    NULL,
};

char* wifi_items[] = {
    "Analizer",
    "Deauth",
    NULL,
};

const char* wifi_analizer_items[] = {
    "Start",
    "Settings",
    NULL,
};

char* wifi_analizer_summary[120] = {
    VERTICAL_SCROLL_TEXT,
    /***************/
    "Summary",
    NULL,
};

char* wifi_analizer_settings_items[] = {
    "Channel",
    "Destination",
    NULL,
};

char* wifi_analizer_summary_question[] = {
    QUESTION_MENU_ITEMS, "Yes", "No", "Show summary?", NULL,
};

char* wifi_analizer_channel_items[] = {
    CONFIGURATION_MENU_ITEMS,
    "[ ] 1",
    "[ ] 2",
    "[ ] 3",
    "[ ] 4",
    "[ ] 5",
    "[ ] 6",
    "[ ] 7",
    "[ ] 8",
    "[ ] 9",
    "[ ] 10",
    "[ ] 11",
    "[ ] 12",
    "[ ] 13",
    "[ ] 14",
    NULL,
};

char* wifi_analizer_destination_items[] = {
    CONFIGURATION_MENU_ITEMS,
    "[ ] SD Card",
    "[ ] Internal",
    NULL,
};

char* bluetooth_items[] = {
    "Trakers scan",
    "Spam",
    NULL,
};

char* zigbee_items[] = {
    "Spoofing",
    "Sniffer",
    NULL,
};

char* zigbee_spoofing_items[] = {
    "Switch",
    "Light",
    NULL,
};

char* thread_items[] = {
    NULL,
};

char* gps_items[] = {
    "Date & Time",
    "Location",
    NULL,
};

char* empty_items[] = {
    NULL,
};

/**
 * @brief List of menu items
 *
 * Used to get the menu items from the menu enum value
 * following the order of the `screen_module_menu_t` enum
 *
 * Usage: menu_items[screen_module_menu_t]
 */
char** menu_items[] = {
    main_items, applications_items, settings_items, about_items,
    /* Applications */
    wifi_items, bluetooth_items, zigbee_items, thread_items,
    empty_items,  // Matter
    gps_items,
    /* WiFi applications */
    wifi_analizer_items,              // WiFi Analizer
    empty_items,                      // WiFi Deauth
    empty_items,                      // WiFi Analizer Start
    wifi_analizer_settings_items,     // WiFi Analizer Settings
    wifi_analizer_summary_question,   // MENU_WIFI_ANALIZER_ASK_SUMMARY
    wifi_analizer_summary,            // WiFi Analizer Summary
    wifi_analizer_channel_items,      // WiFi Analizer Channel
    wifi_analizer_destination_items,  // WiFi Analizer Destination
    /* Bluetooth applications */
    empty_items,  // Bluetooth Trakers scan
    empty_items,  // Bluetooth Spam
    /* Zigbee applications */
    zigbee_spoofing_items,
    empty_items,  // Zigbee Switch
    empty_items,  // Zigbee Light
    empty_items,  // Zigbee Sniffer
    /* Thread applications */
    empty_items,  // Thread CLI
    /* GPS applications */
    empty_items,  // Date & Time
    empty_items,  // Location
    /* About */
    version_text, license_text, credits_text, legal_text,
    /* Settings items */
    empty_items,  // Display
    empty_items,  // Sound
    empty_items,  // System
};
