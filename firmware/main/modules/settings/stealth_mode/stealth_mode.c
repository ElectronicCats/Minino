#include "stealth_mode.h"

#include <stdbool.h>
#include <stdio.h>

#include "buzzer.h"
#include "coroutine.h"
#include "leds.h"
#include "menu_screens_modules.h"
#include "menus_module.h"
#include "modals_module.h"
#include "preferences.h"

static void set_stealth_status() {
  uint8_t stealth_mode = preferences_get_bool("stealth_mode", false);
  char* stealth_options[] = {"Disabled", "Enabled"};
  stealth_mode = modals_module_get_radio_selection(
      stealth_options, "Stealth Mode", stealth_mode);
  preferences_put_bool("stealth_mode", stealth_mode);
  if (stealth_mode) {
    buzzer_disable();
    leds_deinit();
  } else {
    buzzer_enable();
    leds_begin();
  }
  menus_module_restart();
  vTaskDelete(NULL);
}

void stealth_mode_open_menu() {
  start_coroutine(set_stealth_status, NULL);
}