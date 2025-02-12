#include "gps_settings.h"

#include "general_radio_selection.h"
#include "general_submenu.h"
#include "gps_hw.h"
#include "gps_module.h"
#include "menus_module.h"
#include "preferences.h"

static uint8_t last_main_selection = 0;

void gps_settings_main_menu();
void gps_settings_state_menu();
void gps_settings_time_zone();

typedef enum {
  GPS_ENABLE_DISABLE,
  GPS_TIME_ZONE,
} main_options_e;

static const char* main_options[] = {
    "Enable/Disable",
    "Time Zone",
};

static void main_handler(uint8_t selection) {
  last_main_selection = selection;
  switch (selection) {
    case GPS_ENABLE_DISABLE:
      gps_settings_state_menu();
      break;
    case GPS_TIME_ZONE:
      gps_settings_time_zone();
      break;
    default:
      break;
  }
}

void gps_settings_main_menu() {
  general_submenu_menu_t main = {0};
  main.options = main_options;
  main.options_count = sizeof(main_options) / sizeof(char*);
  main.selected_option = last_main_selection;
  main.select_cb = main_handler;
  main.exit_cb = menus_module_exit_app;

  general_submenu(main);
}

typedef enum {
  GPS_DISABLED,
  GPS_ENABLED,
} gps_state_e;

static const char* gps_states[] = {
    "Disabled",
    "Enabled",
};

static void state_handler(uint8_t state) {
  preferences_put_bool(GPS_ENABLED_MEM, state);
  if (state) {
    gps_hw_on();
  } else {
    gps_hw_off();
  }
}

void gps_settings_state_menu() {
  general_radio_selection_menu_t state = {0};
  state.banner = "GPS State";
  state.options = gps_states;
  state.options_count = sizeof(gps_states) / sizeof(char*);
  state.current_option = gps_hw_get_state();
  state.select_cb = state_handler;
  state.style = RADIO_SELECTION_OLD_STYLE;
  state.exit_cb = gps_settings_main_menu;

  general_radio_selection(state);
}

static const char* gps_time_zone_options[] = {
    "UTC-12",   "UTC-11",   "UTC-10",    "UTC-9:30", "UTC-9",    "UTC-8",
    "UTC-7",    "UTC-6",    "UTC-5",     "UTC-4",    "UTC-3:30", "UTC-3",
    "UTC-2",    "UTC-1",    "UTC+0",     "UTC+1",    "UTC+2",    "UTC+3",
    "UTC+3:30", "UTC+4",    "UTC+4:30",  "UTC+5",    "UTC+5:30", "UTC+5:45",
    "UTC+6",    "UTC+6:30", "UTC+7",     "UTC+8",    "UTC+8:45", "UTC+9",
    "UTC+9:30", "UTC+10",   "UTC+10:30", "UTC+11",   "UTC+12",   "UTC+12:45",
    "UTC+13",   "UTC+14"};

void gps_settings_time_zone() {
  general_radio_selection_menu_t time_zone = {0};
  time_zone.banner = "Select Time Zone";
  time_zone.options = gps_time_zone_options;
  time_zone.options_count = sizeof(gps_time_zone_options) / sizeof(char*);
  time_zone.style = RADIO_SELECTION_OLD_STYLE;
  time_zone.current_option = gps_module_get_time_zone();
  time_zone.select_cb = gps_module_set_time_zone;
  time_zone.exit_cb = gps_settings_main_menu;

  general_radio_selection(time_zone);
}