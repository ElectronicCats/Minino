#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

enum MenuLayer {
    LAYER_MAIN_MENU = 0,
    LAYER_APPLICATIONS,
    LAYER_SETTINGS,
    LAYER_ABOUT,
    /* Applications */
    LAYER_WIFI_APPS,
    LAYER_BLUETOOTH_APPS,
    LAYER_ZIGBEE_APPS,
    LAYER_THREAD_APPS,
    LAYER_MATTER_APPS,
    LAYER_GPS,
    /* WiFi applications */
    LAYER_WIFI_ANALIZER,
    /* Bluetooth applications */
    LAYER_BLUETOOTH_AIRTAGS_SCAN,
    /* Thread applications */
    LAYER_THREAD_CLI,
};

typedef enum MenuLayer Layer;

enum MainMenuItem {
    MAIN_MENU_APPLICATIONS = 0,
    MAIN_MENU_SETTINGS,
    MAIN_MENU_ABOUT,
};

enum ApplicationsMenuItem {
    APPLICATIONS_MENU_WIFI = 0,
    APPLICATIONS_MENU_BLUETOOTH,
    APPLICATIONS_MENU_ZIGBEE,
    APPLICATIONS_MENU_THREAD,
    APPLICATIONS_MENU_MATTER,
    APPLICATIONS_MENU_GPS,
};

enum SettingsMenuItem {
    SETTINGS_MENU_DISPLAY = 0,
    SETTINGS_MENU_SOUND,
    SETTINGS_MENU_SYSTEM,
};

enum AboutMenuItem {
    ABOUT_MENU_VERSION = 0,
    ABOUT_MENU_LICENSE,
    ABOUT_MENU_CREDITS,
    ABOUT_MENU_LEGAL,
};

enum WifiMenuItem {
    WIFI_MENU_ANALIZER = 0,
};

enum BluetoothMenuItem {
    BLUETOOTH_MENU_AIRTAGS_SCAN = 0,
};

enum ThreadMenuItem {
    THREAD_MENU_CLI = 0,
};

static char* main_items[] = {
    "Applications",
    "Settings",
    "About",
    NULL,
};

static char* applications_items[] = {
    "WiFi",
    "Bluetooth",
    "Zigbee",
    "Thread",
    "Matter",
    "GPS",
    NULL,
};

static char* settings_items[] = {
    "Display",
    "Sound",
    "System",
    NULL,
};

static char* about_items[] = {
    "Version",
    "License",
    "Credits",
    "Legal",
    NULL,
};

static char* wifi_items[] = {
    "Analizer",
    NULL,
};

static char* bluetooth_items[] = {
    "Airtags scan",
    NULL,
};

static char* zigbee_items[] = {
    "Zigbee CLI",
    NULL,
};

static char* thread_items[] = {
    "Thread CLI",
    NULL,
};

static char* empty_items[] = {
    NULL,
};

// List of menus, it must be in the same order as the enum MenuLayer
static char** menu_items[] = {
    main_items,
    applications_items,
    settings_items,
    about_items,
    /* Applications */
    wifi_items,
    bluetooth_items,
    empty_items,      // Zigbee
    thread_items,
    empty_items,      // Matter
    empty_items,      // GPS
    /* WiFi applications */
    empty_items,      // WiFi Analizer
    /* Bluetooth applications */
    empty_items,      // Bluetooth Airtags scan
    /* Thread applications */
    empty_items,      // Thread CLI
};

#endif  // DISPLAY_HELPER_H