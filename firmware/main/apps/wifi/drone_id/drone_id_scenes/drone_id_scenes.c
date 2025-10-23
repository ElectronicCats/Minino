#include "drone_id_scenes.h"
#include "drone_id.h"
#include "drone_id_preferences.h"
#include "general_knob.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"

void drone_id_scenes_main();

void drone_id_scenes_settings();
void drone_id_scenes_settings_num_drones();
void drone_id_scenes_settings_channel();
void drone_id_scenes_settings_location();
void drone_id_scenes_settings_ble_drone();
void drone_id_scenes_settings_help();

void drone_id_scenes_help();

static uint8_t current_menu = 0;

static uint8_t last_main_selection = 0;
static uint8_t last_settings_selection = 0;

uint8_t drone_id_scenes_get_current() {
  return current_menu;
}

////////////////////////// MAIN //////////////////////////

typedef enum {
  RUN_OPTION,
  SETTINGS_OPTION,
  HELP_OPTION,
} main_menu_options_t;

char* main_menu_options[] = {
    "Run",
    "Settings",
    "Help",
};

static void main_handler(uint8_t option) {
  last_main_selection = option;
  switch (option) {
    case RUN_OPTION:
      current_menu = DRONE_SCENES_RUN;
      drone_id_begin();
      break;
    case SETTINGS_OPTION:
      drone_id_scenes_settings();
      break;
    case HELP_OPTION:
      drone_id_scenes_help();
      break;
    default:
      break;
  }
}

void drone_id_scenes_main() {
  drone_id_preferences_begin();

  general_submenu_menu_t main = {0};
  main.options = main_menu_options;
  main.options_count = sizeof(main_menu_options) / sizeof(char*);
  main.select_cb = main_handler;
  main.selected_option = last_main_selection;
  main.exit_cb = menus_module_restart;

  general_submenu(main);
  current_menu = DRONE_SCENES_MAIN;
}

////////////////////////// SETTINGS //////////////////////////

typedef enum {
  SETTINGS_NUM_DRONES_OPTION,
  SETTINGS_CHANNEL_OPTION,
  SETTINGS_LOCATION_OPTION,
  SETTINGS_BLE_DRONE_OPTION,
  SETTINGS_HELP_OPTION,
} settings_options_e;

static const char* settings_options[] = {"Num of Drones", "Channel",
                                         "Location Src", "BLE Drone", "Help"};

static void settings_handler(uint8_t option) {
  last_settings_selection = option;
  switch (option) {
    case SETTINGS_NUM_DRONES_OPTION:
      drone_id_scenes_settings_num_drones();
      break;
    case SETTINGS_CHANNEL_OPTION:
      drone_id_scenes_settings_channel();
      break;
    case SETTINGS_LOCATION_OPTION:
      drone_id_scenes_settings_location();
      break;
    case SETTINGS_BLE_DRONE_OPTION:
      drone_id_scenes_settings_ble_drone();
      break;
    case SETTINGS_HELP_OPTION:
      drone_id_scenes_settings_help();
      break;
    default:
      break;
  }
}

static void settings_exit_cb() {
  last_settings_selection = 0;
  drone_id_scenes_main();
}

void drone_id_scenes_settings() {
  general_submenu_menu_t settings = {0};

  settings.options = settings_options;
  settings.options_count = sizeof(settings_options) / sizeof(char*);
  settings.select_cb = settings_handler;
  settings.selected_option = last_settings_selection;
  settings.exit_cb = settings_exit_cb;

  general_submenu(settings);
  current_menu = DRONE_SCENES_SETTINGS;
}

static void drone_num_handler(uint8_t num_drones) {
  drone_id_preferences_set_num_drones(num_drones);
  drone_id_set_num_spoofers(num_drones);
}

void drone_id_scenes_settings_num_drones() {
  general_knob_ctx_t num_drones = {0};
  num_drones.help_lbl = "Drone ID Settings";
  num_drones.var_lbl = "Drones Qty";
  num_drones.min = 1;
  num_drones.max = 16;
  num_drones.value = drone_id_preferences_get()->num_drones;
  num_drones.value_handler = drone_num_handler;
  num_drones.step = 1;
  num_drones.exit_cb = drone_id_scenes_settings;

  general_knob(num_drones);
  current_menu = DRONE_SCENES_SETTINGS_NUM_DRONES;
}

static void drone_channel_handler(uint8_t channel) {
  drone_id_preferences_set_channel(channel);
  drone_id_set_wifi_ap(channel);
}
void drone_id_scenes_settings_channel() {
  general_knob_ctx_t channel = {0};
  channel.help_lbl = "Channel Settings";
  channel.var_lbl = "Channel";
  channel.min = 1;
  channel.max = 11;
  channel.value = drone_id_preferences_get()->channel;
  channel.value_handler = drone_channel_handler;
  channel.step = 1;
  channel.exit_cb = drone_id_scenes_settings;

  general_knob(channel);
  current_menu = DRONE_SCENES_SETTINGS_CHANNEL;
}

static const char* location_options[] = {"GPS", "Manual"};

void drone_id_scenes_settings_location() {
  general_radio_selection_menu_t location = {0};
  location.banner = "Location Src";
  location.options = location_options;
  location.options_count = sizeof(location_options) / sizeof(char*);
  location.current_option = drone_id_preferences_get()->location_source;
  location.style = RADIO_SELECTION_OLD_STYLE;
  location.select_cb = drone_id_set_location_source;
  location.exit_cb = drone_id_scenes_settings;

  general_radio_selection(location);
  current_menu = DRONE_SCENES_SETTINGS_LOCATION_SRC;
}

static const char* ble_drone_options[] = {"Disabled", "Enabled"};

void drone_id_scenes_settings_ble_drone() {
  general_radio_selection_menu_t ble_drone = {0};
  ble_drone.banner = "BLE Drone";
  ble_drone.options = ble_drone_options;
  ble_drone.options_count = sizeof(ble_drone_options) / sizeof(char*);
  ble_drone.current_option = drone_id_preferences_get()->add_ble_drone;
  ble_drone.style = RADIO_SELECTION_OLD_STYLE;
  ble_drone.select_cb = drone_id_set_ble_drone;
  ble_drone.exit_cb = drone_id_scenes_settings;

  general_radio_selection(ble_drone);
  current_menu = DRONE_SCENES_SETTINGS_BLE_DRONE;
}

static const char* settings_help_txt[] = {"Num of Drones:",
                                          "Set the number",
                                          "of drones to",
                                          "spoof",
                                          "",
                                          "Channel:",
                                          "Set the wifi",
                                          "channel to use",
                                          "",
                                          "Location Src:",
                                          "Set the source",
                                          "of location to",
                                          "gps or manual"
                                          "",
                                          "You can set",
                                          "the manual",
                                          "location by",
                                          "using the",
                                          "`drone_set_",
                                          "location'",
                                          "command"};

void drone_id_scenes_settings_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Settings Help";
  help.text_arr = settings_help_txt;
  help.text_len = sizeof(settings_help_txt) / sizeof(char*);
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.exit_cb = drone_id_scenes_settings;

  general_scrolling_text_array(help);
  current_menu = DRONE_SCENES_SETTINGS_HELP;
}

////////////////////////// HELP //////////////////////////

static char* help_txt[] = {"This app allows", "you to spoof",
                                 "drone IDs",       "using ESP32",
                                 "wifi features",   "",
                                 "You can set",     "the number",
                                 "of drones,",      "the wifi",
                                 "channel and",     "the location",
                                 "source.",         "",
                                 "Please note",     "that this app",
                                 "if for",          "educational",
                                 "purposes only.",  "",
                                 "Familiarizing",   "with (FAA) laws",
                                 "is crucial to",   "avoid legal",
                                 "consequences."};

void drone_id_scenes_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Drone ID Help";
  help.text_arr = (const char**) help_txt;
  help.text_len = sizeof(help_txt) / sizeof(char*);
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.exit_cb = drone_id_scenes_main;

  // general_scrolling_text(help);
  general_scrolling_text_array(help);
  current_menu = DRONE_SCENES_HELP;
}
