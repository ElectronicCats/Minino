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

static char* main_options[] = {
    "Applications",
    "Settings",
    "About",
};

static char* applications_options[] = {
    "WiFi",
    "Bluetooth",
    "Zigbee",
    "Thread",
    "Matter",
    "GPS",
};

static char* settings_options[] = {
    "Display",
    "Sound",
    "System",
};

static char* about_options[] = {
    "Version",
    "License",
    "Credits",
    "Legal",
};

static char* wifi_options[] = {
    "Analizer",
};

#endif  // DISPLAY_HELPER_H