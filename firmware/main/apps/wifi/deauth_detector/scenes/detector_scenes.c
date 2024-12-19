#include "detector_scenes.h"

#include "general_radio_selection.h"
#include "general_submenu.h"
#include "menus_module.h"

void detector_scenes_main_menu();
void detector_scenes_settings();
void detector_scenes_channel();
void detector_scenes_help();

//////////////////////////   Main Menu   //////////////////////////
static enum { RUN_OPTION, SETTINGS_OPTION, HELP_OPTION } main_menu_options_e;
static const char* main_menu_options[] = {"Run", "Settings", "Help"};
static void main_menu_handler(uint8_t selection) {
  switch (selection) {
    case RUN_OPTION:
      // DEAUTH DETECTOR BEGIN
      break;
    case SETTINGS_OPTION:
      detector_scenes_settings();
      break;
    case HELP_OPTION:
      detector_scenes_help();
      break;
    default:
      break;
  }
}

static void main_menu_exit() {
  menus_module_exit_app();
}

void detector_scenes_main_menu() {
  general_submenu_menu_t main_menu;
  main_menu.options = main_menu_options;
  main_menu.options_count = sizeof(main_menu_options) / sizeof(char*);
  main_menu.select_cb = main_menu_handler;
  main_menu.exit_cb = main_menu_exit;
  general_submenu(main_menu);
}

//////////////////////////   Settings Menu   //////////////////////////
static enum { CHANNEL_HOP_OPTION, CHANNEL_OPTION } settings_options_e;
static const char* settings_options[] = {"Channel hop", "Channel"};
static void settings_handler(uint8_t scan_mode) {
  // TODO: SAVE "scan_mode" TO FLASH
  // TODO: SET SCAN MODE TO "scan_mode"
  switch (scan_mode) {
    case CHANNEL_HOP_OPTION:
      break;
    case CHANNEL_OPTION:
      detector_scenes_channel();
      break;
    default:
      break;
  }
}

static void settings_exit() {
  detector_scenes_main_menu();
}

void detector_scenes_settings() {
  general_radio_selection_menu_t settings;
  settings.banner = "Scan Mode";
  settings.options = settings_options;
  settings.options_count = sizeof(settings_options) / sizeof(char*);
  settings.select_cb = settings_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = settings_exit;
  settings.current_option = 0;  // TODO: GET "scan_mode" FROM FLASH
  general_radio_selection(settings);
}

//////////////////////////   Channel Hop Menu   //////////////////////////
static const char* channel_options[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
};
static void channel_handler(uint8_t channel) {
  // TODO: Set Deauth Detector Channel to "channel"
  // TODO: SAVE "channel" TO FLASH
}

static void channel_exit() {
  detector_scenes_settings();
}

void detector_scenes_channel() {
  general_radio_selection_menu_t settings;
  settings.banner = "Select Channel";
  settings.options = channel_options;
  settings.options_count = sizeof(channel_options) / sizeof(char*);
  settings.select_cb = channel_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = channel_exit;
  settings.current_option = 0;  // TODO: GET "channel" FROM FLASH
  general_radio_selection(settings);
}

//////////////////////////   Help Menu   //////////////////////////

static void help_exit() {}

void detector_scenes_help() {}