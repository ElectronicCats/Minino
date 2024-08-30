#include "stealth_mode.h"

#include <stdbool.h>
#include <stdio.h>

#include "buzzer.h"
#include "coroutine.h"
#include "general_radio_selection.h"
#include "leds.h"
#include "menus_module.h"
#include "modals_module.h"
#include "preferences.h"

char* stealth_mode_options[] = {"Disabled", "Enabled"};

static void stealth_selection_handler(uint8_t stealth_mode) {
  preferences_put_bool("stealth_mode", stealth_mode);
  if (stealth_mode) {
    buzzer_disable();
    leds_deinit();
  } else {
    buzzer_enable();
    leds_begin();
  }
}

void stealth_mode_open_menu() {
  general_radio_selection_menu_t stealth_mode_radio_menu;
  stealth_mode_radio_menu.banner = "Stealth Mode";
  stealth_mode_radio_menu.exit_cb = menus_module_exit_app;
  stealth_mode_radio_menu.options = stealth_mode_options;
  stealth_mode_radio_menu.options_count = 2;
  stealth_mode_radio_menu.style = RADIO_SELECTION_OLD_STYLE;
  stealth_mode_radio_menu.current_option =
      preferences_get_bool("stealth_mode", false);
  stealth_mode_radio_menu.select_cb = stealth_selection_handler;
  general_radio_selection(stealth_mode_radio_menu);
}