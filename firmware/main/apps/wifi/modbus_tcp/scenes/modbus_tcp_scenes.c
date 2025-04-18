#include "modbus_tcp_scenes.h"

#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"

#include "modbus_attacks.h"
#include "modbus_dos.h"
#include "modbus_dos_prefs.h"
#include "modbus_dos_screens.h"
#include "modbus_engine.h"

#include "wifi_ap_manager.h"

#include "esp_log.h"

static uint8_t last_main_selection = 0;
static uint8_t last_settings_selection = 0;
static char* list_ap[20];

void modbus_tcp_scenes_help();

static void show_cmds_help(char* head, char* body);
static void show_no_connection_notify();

typedef enum {
  MAIN_OPTIONS_CONNECT,
  MAIN_OPTIONS_TARGET,
  MAIN_OPTOINS_RUN,
  MAIN_OPTIONS_SETTINGS,
  MAIN_OPTIONS_HELP,
} main_options_t;

typedef enum {
  MBUS_ATTACK_DOS,
  MBUS_ATTACK_WRITE,
} mbus_attack_t;

static const char* main_options[] = {"Connect", "Target", "Run", "Commands",
                                     "Help"};
static const char* attacks_options[] = {"DoS", "Write Request"};

void modbus_tcp_scenes_entry() {
  modbus_tcp_scenes_main();
}

static void modbus_tcp_scenes_show_target() {
  modbus_engine_t* mbus_ctx = modbus_engine_get_ctx();
  if (mbus_ctx == NULL) {
    show_no_connection_notify();
    return;
  }
  char* body_text[100];
  if (mbus_ctx->ip == NULL) {
    ESP_LOGW("TAG", "Something went wrong");
    modbus_tcp_scenes_main();
  }
  sprintf(body_text, "IP: %s Port: %d", mbus_ctx->ip, mbus_ctx->port);
  general_notification_ctx_t notification = {0};
  notification.head = "Target";
  notification.body = body_text;
  notification.on_exit = modbus_tcp_scenes_main;
  general_notification(notification);
}

static void handle_attack_selector(uint8_t option) {
  switch (option) {
    case MBUS_ATTACK_DOS:
      modbus_attacks_writer();
      break;
    case MBUS_ATTACK_WRITE:

      break;
    default:
      break;
  }
}

static void modbus_tcp_scenes_show_attacks() {
  general_submenu_menu_t submenu = {0};
  submenu.options = attacks_options;
  submenu.options_count = sizeof(attacks_options) / sizeof(char*);
  submenu.select_cb = handle_attack_selector;
  submenu.selected_option = 0;
  submenu.exit_cb = modbus_tcp_scenes_main;

  general_submenu(submenu);
}

static void handle_tcp_attacks() {
  if (!wifi_ap_manager_is_connect()) {
    show_no_connection_notify();
    return;
  }
  modbus_engine_t* mbus_ctx = modbus_engine_get_ctx();
  if (mbus_ctx == NULL) {
    modbus_engine_begin();
  }

  modbus_tcp_scenes_show_attacks();
}

static void handle_cb_ap_connection(bool state) {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 8000;
  notification.head = "Wifi";
  if (state) {
    notification.body = "Connected";
    main_options[0] = "Connected";
    modbus_engine_begin();
  } else {
    notification.body = "Error";
  }
  general_notification(notification);
  modbus_tcp_scenes_main();
}

static void handle_tcp_ap_connection(uint8_t option) {
  wifi_ap_manager_connect_index_cb(option, handle_cb_ap_connection);
}

static void wifi_zero_aps() {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 2000;
  notification.head = "Wifi";
  notification.body = "No APs, add with the command save";
  general_notification(notification);
  modbus_tcp_scenes_main();
}

void modbus_tcp_scenes_ap_connect() {
  int count = wifi_ap_manager_get_count();
  if (count == 0) {
    wifi_zero_aps();
    return;
  }
  wifi_ap_manager_get_aps(list_ap);
  general_submenu_menu_t submenu = {0};
  submenu.options = list_ap;
  submenu.options_count = count;
  submenu.select_cb = handle_tcp_ap_connection;
  submenu.selected_option = last_settings_selection;
  submenu.exit_cb = modbus_tcp_scenes_main;

  general_submenu(submenu);
}

static void main_handler(uint8_t option) {
  last_main_selection = option;
  switch (option) {
    case MAIN_OPTIONS_CONNECT:
      modbus_tcp_scenes_ap_connect();
      break;
    case MAIN_OPTIONS_TARGET:
      modbus_tcp_scenes_show_target();
      break;
    case MAIN_OPTOINS_RUN:
      handle_tcp_attacks();
      break;
    case MAIN_OPTIONS_SETTINGS:
      modbus_tcp_scenes_settings();
      break;
    case MAIN_OPTIONS_HELP:
      modbus_tcp_scenes_help();
      break;
    default:
      break;
  }
}

static void main_exit_cb() {
  menus_module_restart();
}

void modbus_tcp_scenes_main() {
  general_submenu_menu_t submenu = {0};
  submenu.options = main_options;
  submenu.options_count = sizeof(main_options) / sizeof(char*);
  submenu.select_cb = main_handler;
  submenu.selected_option = last_main_selection;
  submenu.exit_cb = main_exit_cb;

  general_submenu(submenu);

  modbus_dos_prefs_begin();
}

typedef enum {
  SETTINGS_OPTIONS_CONNECT,
  SETTINGS_OPTOINS_IP,
  SETTINGS_OPTIONS_PORT,
} setting_options_t;

static const char* settings_options[] = {"Connect", "Server IP", "Server Port"};

static void show_cmds_help(char* head, char* body) {
  general_notification_ctx_t notification = {0};
  notification.head = head;
  notification.body = body;
  notification.on_exit = modbus_tcp_scenes_settings;

  general_notification_handler(notification);
}

static void show_no_connection_notify() {
  general_notification_ctx_t notification = {0};
  notification.head = "Alert";
  notification.body = "First connect to an Access Point";
  notification.on_exit = modbus_tcp_scenes_main;

  general_notification_handler(notification);
}

static void settings_handler(uint8_t option) {
  last_settings_selection = option;
  switch (option) {
    case SETTINGS_OPTIONS_CONNECT:
      show_cmds_help("Connect", "CMD: connect <index of AP>");
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
  modbus_tcp_scenes_main();
}

void modbus_tcp_scenes_settings() {
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

void modbus_tcp_scenes_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Modbus DOS Help";
  help.text_arr = help_txt;
  help.text_len = sizeof(help_txt) / sizeof(char*);
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.exit_cb = modbus_tcp_scenes_main;

  general_scrolling_text_array(help);
}