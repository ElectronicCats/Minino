#include "sleep_mode_scenes.h"

#include "general_knob.h"
#include "general_radio_selection.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"
#include "sleep_mode.h"

#define AFK_TIME_MEM          "afk_time"
#define SLEEP_MODE_ENABLE_MEM "sleep_enable"

static uint8_t last_main_selection = 0;

void sleep_mode_scenes_main();
void sleep_mode_scenes_enable();
void sleep_mode_scenes_time();

static const char* main_options[] = {"Enable/Disable", "Set sleep time"};

typedef enum {
  SLEEP_MODE_ENABLE,
  SLEEP_MODE_TIME,
} sleep_mode_main_options_e;

static void main_handler(uint8_t selection) {
  last_main_selection = selection;
  switch (selection) {
    case SLEEP_MODE_ENABLE:
      sleep_mode_scenes_enable();
      break;
    case SLEEP_MODE_TIME:
      sleep_mode_scenes_time();
      break;

    default:
      break;
  }
}

void sleep_mode_scenes_main() {
  general_submenu_menu_t main;
  main.options = main_options;
  main.options_count = sizeof(main_options) / sizeof(char*);
  main.selected_option = last_main_selection;
  main.select_cb = main_handler;
  main.exit_cb = menus_module_exit_app;

  general_submenu(main);
}

static const char* enable_options[] = {"Disable", "Enable"};

static void enable_handler(uint8_t selection) {
  sleep_mode_set_enabled(selection);
}

void sleep_mode_scenes_enable() {
  general_radio_selection_menu_t enable;
  enable.banner = "Sleep Mode";
  enable.options = enable_options;
  enable.options_count = sizeof(enable_options) / sizeof(char*);
  enable.current_option = preferences_get_bool(SLEEP_MODE_ENABLE_MEM, 1);
  enable.style = RADIO_SELECTION_OLD_STYLE;
  enable.select_cb = enable_handler;
  enable.exit_cb = sleep_mode_scenes_main;

  general_radio_selection(enable);
}

void sleep_mode_scenes_time() {
  general_knob_ctx_t knob = {0};
  knob.min = 10;
  knob.max = 300;
  knob.step = 10;
  knob.value = preferences_get_short(AFK_TIME_MEM, 10);
  knob.var_lbl = "Sleep Time";
  knob.help_lbl = "Time in Seconds";
  knob.value_handler = sleep_mode_set_afk_timeout;
  knob.exit_cb = sleep_mode_scenes_main;

  general_knob(knob);
}