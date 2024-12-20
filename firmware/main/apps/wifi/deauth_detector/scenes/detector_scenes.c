#include "detector_scenes.h"
#include "detector.h"

#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

void detector_scenes_main_menu();
void detector_scenes_settings();
void detector_scenes_channel();
void detector_scenes_help();

static void detector_scenes_scanning() {
  oled_screen_clear();
  oled_screen_display_text_center("Channel | Count", 0, OLED_DISPLAY_NORMAL);
}

void detector_scenes_show_count(uint16_t count, uint8_t channel) {
  oled_screen_clear_buffer();
  oled_screen_display_text_center("Scanning", 0, OLED_DISPLAY_NORMAL);
  char* str = malloc(40);
  sprintf(str, "Total packets: %d", count);
  oled_screen_display_text_center(str, 1, OLED_DISPLAY_NORMAL);
  char* str2 = malloc(40);
  sprintf(str2, "Channel: %d", channel);
  oled_screen_display_text_center(str2, 2, OLED_DISPLAY_NORMAL);
  free(str);
  free(str2);
  oled_screen_display_show();
}

void detector_scenes_show_table(uint16_t* deauth_packets_count_list) {
  oled_screen_clear_buffer();
  oled_screen_display_text_center("Channel | Count", 0, OLED_DISPLAY_NORMAL);
  for (int i = 0; i < 14; i += 2) {
    char* str = malloc(40);
    if (i + 1 < 14) {  // Verifica que no se salga del rango
      sprintf(str, "%d: %d  | %d: %d", i + 1, deauth_packets_count_list[i],
              i + 2, deauth_packets_count_list[i + 1]);
    } else {  // Caso especial si es el último canal y no hay un par
      sprintf(str, "%d: %d", i + 1, deauth_packets_count_list[i]);
    }
    oled_screen_display_text(
        str, 0, (i / 2) + 1,
        OLED_DISPLAY_NORMAL);  // Ajusta la posición vertical
    free(str);
  }
  oled_screen_display_show();
}

//////////////////////////   Main Menu   //////////////////////////
static enum { RUN_OPTION, SETTINGS_OPTION, HELP_OPTION } main_menu_options_e;
static const char* main_menu_options[] = {"Run", "Settings", "Help"};
static void main_menu_handler(uint8_t selection) {
  switch (selection) {
    case RUN_OPTION:
      // DEAUTH DETECTOR BEGIN
      deauth_detector_begin();
      detector_scenes_scanning();
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
      preferences_put_int("det_channel", 99);
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
  uint8_t get_saved_channel = preferences_get_int("det_channel", 99);
  settings.current_option =
      get_saved_channel == 99 ? CHANNEL_HOP_OPTION : CHANNEL_OPTION;
  general_radio_selection(settings);
}

//////////////////////////   Channel Hop Menu   //////////////////////////
static const char* channel_options[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
};
static void channel_handler(uint8_t channel) {
  preferences_put_int("det_channel", channel);
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
  uint8_t get_saved_channel = preferences_get_int("det_channel", 99);
  settings.current_option = get_saved_channel == 99 ? 0 : get_saved_channel;
  preferences_put_int("det_channel", settings.current_option);
  general_radio_selection(settings);
}

//////////////////////////   Help Menu   //////////////////////////

static const char* help_text =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat.";

static void help_exit() {
  detector_scenes_main_menu();
}

void detector_scenes_help() {
  general_scrolling_text_ctx help;
  memset(&help, 0, sizeof(help));
  help.banner = "< Back";
  help.text = help_text;
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.exit_cb = help_exit;
  general_scrolling_text(help);
}