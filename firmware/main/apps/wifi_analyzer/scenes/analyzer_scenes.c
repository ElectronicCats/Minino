#include "analyzer_scenes.h"

#include "general_radio_selection.h"
#include "general_screens.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "wifi_analyzer.h"
#include "wifi_sniffer.h"

static analyzer_scenes_e current_scene;

void analyzer_scenes_main_menu();
void analyzer_scenes_settings();
void analyzer_scenes_channel();
void analyzer_scenes_destination();

analyzer_scenes_e analyzer_get_current_scene() {
  return current_scene;
}

//////////////////////////   MAIN MENU   ///////////////////////////////
static enum {
  ANALYZER_START_OPTION,
  ANALYZER_SETTINGS_OPTION,
  ANALYZER_HELP_OPTION
} analyzer_main_options_e;

const char* analizer_main_options[] = {"Start", "Settings", "Help"};

static void main_menu_selection_handler(uint8_t selection) {
  switch (selection) {
    case ANALYZER_START_OPTION:
      wifi_analyzer_run();
      break;
    case ANALYZER_SETTINGS_OPTION:
      analyzer_scenes_settings();
      break;
    case ANALYZER_HELP_OPTION:
      wifi_analyzer_help();
      break;
    default:
      break;
  }
}

static void main_menu_exit_handler() {
  menus_module_reset();
}

void analyzer_scenes_main_menu() {
  current_scene = ANALYZER_MAIN_SCENE;
  general_submenu_menu_t main_menu = {0};
  main_menu.options = analizer_main_options;
  main_menu.options_count = sizeof(analizer_main_options) / sizeof(char*);
  main_menu.select_cb = main_menu_selection_handler;
  main_menu.exit_cb = main_menu_exit_handler;
  general_submenu(main_menu);
  wifi_analyzer_begin();
}

//////////////////////////   SETTINGS MENU   ///////////////////////////////
static enum {
  ANALYZER_SETTINGS_CHANNEL_OPTION,
  ANALYZER_SETTINGS_DESTINATION_OPTION,
} analyzer_settings_options_e;

const char* analizer_settings_options[] = {"Channel", "Destination"};

static void settings_selection_handler(uint8_t selection) {
  switch (selection) {
    case ANALYZER_SETTINGS_CHANNEL_OPTION:
      analyzer_scenes_channel();
      break;
    case ANALYZER_SETTINGS_DESTINATION_OPTION:
      analyzer_scenes_destination();
      break;
    default:
      break;
  }
}

static void settings_exit_handler() {
  analyzer_scenes_main_menu();
}

void analyzer_scenes_settings() {
  current_scene = ANALYZER_SETTINGS_OPTION;
  general_submenu_menu_t settings_menu = {0};
  settings_menu.options = analizer_settings_options;
  settings_menu.options_count =
      sizeof(analizer_settings_options) / sizeof(char*);
  settings_menu.select_cb = settings_selection_handler;
  settings_menu.exit_cb = settings_exit_handler;
  general_submenu(settings_menu);
}
//////////////////////////   CHANNEL MENU   ///////////////////////////////
static const char* channel_options[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
};

static void set_channel(uint8_t selected_item) {
  wifi_sniffer_set_channel(selected_item + 1);
}

void analyzer_scenes_channel() {
  general_radio_selection_menu_t channel = {0};
  channel.banner = "Choose Channel",
  channel.current_option = wifi_sniffer_get_channel() - 1;
  channel.options = channel_options;
  channel.options_count = sizeof(channel_options) / sizeof(char*);
  channel.select_cb = set_channel;
  channel.exit_cb = analyzer_scenes_settings;
  channel.style = RADIO_SELECTION_OLD_STYLE;
  general_radio_selection(channel);
}

//////////////////////////   DESTINATION MENU   ///////////////////////////////
static const char* destination_options[] = {"SD", "Internal"};
static void set_destination(uint8_t selected_item) {
  if (selected_item == WIFI_SNIFFER_DESTINATION_SD) {
    wifi_sniffer_set_destination_sd();
  } else {
    wifi_sniffer_set_destination_internal();
  }
}

static void destination_scene_exit() {
  wifi_module_analyzer_destination_exit();
  analyzer_scenes_settings();
}
void analyzer_scenes_destination() {
  general_radio_selection_menu_t destination = {0};
  destination.banner = "Choose Destination",
  destination.current_option = wifi_sniffer_is_destination_internal();
  destination.options = destination_options;
  destination.options_count = sizeof(destination_options) / sizeof(char*);
  destination.select_cb = set_destination;
  destination.exit_cb = destination_scene_exit;
  destination.style = RADIO_SELECTION_OLD_STYLE;
  general_radio_selection(destination);
}

static char* wifi_analizer_help[] = {
    "This tool",      "allows you to",   "analyze the",
    "WiFi networks",  "around you.",     "",
    "You can select", "the channel and", "the destination",
    "to save the",    "results.",
};

static const general_menu_t analyzer_help_menu = {
    .menu_items = wifi_analizer_help,
    .menu_count = 11,
    .menu_level = GENERAL_TREE_APP_MENU};

void wifi_analyzer_help() {
  general_register_scrolling_menu(&analyzer_help_menu);
  general_screen_display_scrolling_text_handler(analyzer_scenes_main_menu);
}