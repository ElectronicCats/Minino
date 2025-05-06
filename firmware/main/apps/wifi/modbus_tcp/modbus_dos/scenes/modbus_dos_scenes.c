#include "modbus_dos_scenes.h"

#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"

#include "modbus_dos.h"
#include "modbus_dos_prefs.h"
#include "modbus_dos_screens.h"
#include "modbus_engine.h"

static uint8_t last_main_selection = 0;
static uint8_t last_settings_selection = 0;

void modbus_dos_scenes_main();
void modbus_dos_scenes_settings();
void modbus_dos_scenes_help();

static void conf_missing_notification();
static void show_cmds_help(const char* head, const char* body);

typedef enum {
  MAIN_OPTOINS_RUN,
  MAIN_OPTIONS_SETTINGS,
  MAIN_OPTIONS_HELP,
} main_options_t;

static const char* main_options[] = {"Run", "commands", "Help"};

static void main_handler(uint8_t option) {
  last_main_selection = option;
  switch (option) {
    case MAIN_OPTOINS_RUN:
      if (modbus_dos_prefs_check()) {
        // modbus_dos_begin();
        modbus_engine_begin();
        modbus_dos_screens_main();
      } else {
        conf_missing_notification();
      }
      break;
    case MAIN_OPTIONS_SETTINGS:
      modbus_dos_scenes_settings();
      break;
    case MAIN_OPTIONS_HELP:
      modbus_dos_scenes_help();
      break;
    default:
      break;
  }
}

static void main_exit_cb() {
  menus_module_restart();
}

void modbus_dos_scenes_main() {
  general_submenu_menu_t submenu = {0};
  submenu.options = main_options;
  submenu.options_count = sizeof(main_options) / sizeof(char*);
  submenu.select_cb = main_handler;
  submenu.selected_option = last_main_selection;
  submenu.exit_cb = main_exit_cb;

  general_submenu(submenu);

  modbus_dos_prefs_begin();
}

static void conf_missing_notification() {
  general_notification_ctx_t notification = {0};
  notification.head = "Missing conf";
  notification.body = "You must setup the server first";
  notification.on_exit = modbus_dos_scenes_settings;

  general_notification_handler(notification);
}

typedef enum {
  SETTINGS_OPTIONS_SSID,
  SETTINGS_OPTIONS_PASS,
  SETTINGS_OPTOINS_IP,
  SETTINGS_OPTIONS_PORT,
} setting_options_t;

static const char* settings_options[] = {"SSID", "Password", "Server IP",
                                         "Server Port"};

static void show_cmds_help(const char* head, const char* body) {
  general_notification_ctx_t notification = {0};
  notification.head = head;
  notification.body = body;
  notification.on_exit = modbus_dos_scenes_settings;

  general_notification_handler(notification);
}

static void settings_handler(uint8_t option) {
  last_settings_selection = option;
  switch (option) {
    case SETTINGS_OPTIONS_SSID:
      show_cmds_help("SSID", "CMD: mb_dos_set_ssid");
      break;
    case SETTINGS_OPTIONS_PASS:
      show_cmds_help("Password", "CMD: mb_dos_set_pass");
      break;
    case SETTINGS_OPTOINS_IP:
      show_cmds_help("Server IP", "CMD: mb_dos_set_ip");
      break;
    case SETTINGS_OPTIONS_PORT:
      show_cmds_help("Server Port", "CMD: mb_dos_set_port");
      break;
    default:
      break;
  }
}

static void settings_exit_cb() {
  modbus_dos_scenes_main();
}

void modbus_dos_scenes_settings() {
  general_submenu_menu_t submenu = {0};
  submenu.options = settings_options;
  submenu.options_count = sizeof(settings_options) / sizeof(char*);
  submenu.select_cb = settings_handler;
  submenu.selected_option = last_settings_selection;
  submenu.exit_cb = settings_exit_cb;

  general_submenu(submenu);
}

static const char* help_txt[] = {"You must set",   "the server",
                                 "settings using", "the commands",
                                 "specified in",   "`commands' menu"};

void modbus_dos_scenes_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Modbus DOS Help";
  help.text_arr = help_txt;
  help.text_len = sizeof(help_txt) / sizeof(char*);
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.exit_cb = modbus_dos_scenes_main;

  general_scrolling_text_array(help);
}