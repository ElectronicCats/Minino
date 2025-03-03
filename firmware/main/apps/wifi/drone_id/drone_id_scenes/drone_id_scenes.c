#include "drone_id_scenes.h"

#include "drone_id.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"

void drone_id_scenes_main();
void drone_id_scenes_help();

static uint8_t last_main_selection = 0;

typedef enum {
  RUN_OPTION,
  HELP_OPTION,
} main_menu_options_t;

static const char* main_menu_options[] = {
    "Run",
    "Help",
};

static void main_handler(uint8_t option) {
  switch (option) {
    case RUN_OPTION:
      drone_id_begin();
      break;
    case HELP_OPTION:
      drone_id_scenes_help();
      break;
    default:
      break;
  }
}

void drone_id_scenes_main() {
  general_submenu_menu_t main = {0};
  main.options = main_menu_options;
  main.options_count = sizeof(main_menu_options) / sizeof(char*);
  main.select_cb = main_handler;
  main.selected_option = last_main_selection;
  main.exit_cb = menus_module_restart;

  general_submenu(main);
}

void drone_id_scenes_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Drone ID Hep";
  help.text = "Drone ID Help";
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.exit_cb = drone_id_scenes_main;

  general_scrolling_text(help);
}
