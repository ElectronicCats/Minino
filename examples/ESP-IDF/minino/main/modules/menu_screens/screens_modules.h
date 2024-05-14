#pragma once

#include <stddef.h>

#define SCROLLING_TEXT "scrollable"

typedef enum {
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
  LAYER_WIFI_DEAUTH,
  /* WiFi analizer items */
  LAYER_WIFI_ANALIZER_START,
  LAYER_WIFI_ANALIZER_SETTINGS,
  /* Bluetooth applications */
  LAYER_BLUETOOTH_AIRTAGS_SCAN,
  /* Zigbee applications */
  LAYER_ZIGBEE_SPOOFING,
  LAYER_ZIGBEE_SWITCH,
  LAYER_ZIGBEE_LIGHT,
  /* Thread applications */
  LAYER_THREAD_CLI,
  /* GPS applications */
  LAYER_GPS_DATE_TIME,
  LAYER_GPS_LOCATION,
  /* About items */
  LAYER_ABOUT_VERSION,
  LAYER_ABOUT_LICENSE,
  LAYER_ABOUT_CREDITS,
  LAYER_ABOUT_LEGAL,
  /* Settings items */
  LAYER_SETTINGS_DISPLAY,
  LAYER_SETTINGS_SOUND,
  LAYER_SETTINGS_SYSTEM,
  /* About submenus */
} screen_module_layer_t;

enum menu_layer_main_items {
  MAIN_MENU_APPLICATIONS = 0,
  MAIN_MENU_SETTINGS,
  MAIN_MENU_ABOUT,
};

enum menu_layer_applications_items {
  APPLICATIONS_MENU_WIFI = 0,
  APPLICATIONS_MENU_BLUETOOTH,
  APPLICATIONS_MENU_ZIGBEE,
  APPLICATIONS_MENU_THREAD,
  APPLICATIONS_MENU_MATTER,
  APPLICATIONS_MENU_GPS,
};

enum menu_layer_settings_items {
  SETTINGS_MENU_DISPLAY = 0,
  SETTINGS_MENU_SOUND,
  SETTINGS_MENU_SYSTEM,
};

enum menu_layer_about_items {
  ABOUT_MENU_VERSION = 0,
  ABOUT_MENU_LICENSE,
  ABOUT_MENU_CREDITS,
  ABOUT_MENU_LEGAL,
};

enum menu_layer_wifi_items {
  WIFI_MENU_ANALIZER = 0,
  WIFI_MENU_DEAUTH,
};

enum menu_layer_wifi_sniffer_items {
  WIFI_SNIFFER_START = 0,
  WIFI_SNIFFER_SETTINGS,
};

enum menu_layer_bluetooth_items {
  BLUETOOTH_MENU_AIRTAGS_SCAN = 0,
};

enum menu_layer_zigbee_items {
  ZIGBEE_MENU_SPOOFING = 0,
};

enum menu_layer_zigbee_spoofing_items {
  ZIGBEE_SPOOFING_SWITCH = 0,
  ZIGBEE_SPOOFING_LIGHT,
};

enum menu_layer_zigbee_switch_items {
  ZIGBEE_SWITCH_TOGGLE = 0,
};

enum menu_layer_thread_items {
  THREAD_MENU_CLI = 0,
};

enum menu_layer_gps_items {
  GPS_MENU_DATE_TIME = 0,
  GPS_MENU_LOCATION,
};

static char* main_items[] = {
    "Applications",
    "Settings",
    "About",
    NULL,
};

static char* applications_items[] = {
    "WiFi", "Bluetooth", "Zigbee", "Thread", "Matter", "GPS", NULL,
};

static char* settings_items[] = {
    "Display",
    "Sound",
    "System",
    NULL,
};

static char* about_items[] = {
    "Version", "License", "Credits", "Legal", NULL,
};

static char* version_text[] = {
    SCROLLING_TEXT,
    /***************/
    "",
    "",
    "",
    " Minino v1.3.0",
    "     BETA",
    NULL,
};

static char* license_text[] = {
    SCROLLING_TEXT,
    /***************/
    "",
    "",
    "",
    "  GNU GPL 3.0",
    NULL,
};

static char* credits_text[] = {
    SCROLLING_TEXT,
    /***************/
    "Developed by",
    "Electronic Cats",
    "",
    "This product is",
    "in a BETA stage",
    "use at your own",
    NULL,
};

static char* legal_text[] = {
    SCROLLING_TEXT,
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

static char* wifi_items[] = {
    "Analizer",
    "Deauth",
    NULL,
};

static char* wifi_analizer_items[] = {
    "Start",
    "Settings",
    NULL,
};

static char* wifi_sniffer_settings_items[] = {
    "Channel",
    "Filter",
    NULL,
};

static char* bluetooth_items[] = {
    "Airtags scan",
    NULL,
};

static char* zigbee_items[] = {
    "Spoofing",
    NULL,
};

static char* zigbee_spoofing_items[] = {
    "Switch",
    "Light",
    NULL,
};

static char* thread_items[] = {
    "Thread CLI",
    NULL,
};

static char* gps_items[] = {
    "Date & Time",
    "Location",
    NULL,
};

static char* empty_items[] = {
    NULL,
};

// List of menus, it must be in the same order as the enum screen_module_layer_t
static char** menu_items[] = {
    main_items, applications_items, settings_items, about_items,
    /* Applications */
    wifi_items, bluetooth_items, zigbee_items, thread_items,
    empty_items,  // Matter
    gps_items,
    /* WiFi applications */
    wifi_analizer_items,
    empty_items,  // WiFi Deauth
    empty_items,  // WiFi Analizer Start
    empty_items,  // WiFi Analizer Settings
    /* Bluetooth applications */
    empty_items,  // Bluetooth Airtags scan
    /* Zigbee applications */
    zigbee_spoofing_items,
    empty_items,  // Zigbee Switch
    empty_items,  // Zigbee Light
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
