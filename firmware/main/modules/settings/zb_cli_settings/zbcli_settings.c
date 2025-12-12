#include "zbcli_settings.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"

static const char* settings_main[] = {"Disable", "Enable"};

static void zbcli_handle_selector(uint8_t option) {
  preferences_put_int("ZBCLI", option);
  menus_module_reset();
}

void zbcli_settings_main() {
  general_radio_selection_menu_t setting = {0};
  setting.banner = "Zigbee CLI";
  setting.options = settings_main;
  setting.options_count = sizeof(settings_main) / sizeof(char*);
  setting.select_cb = zbcli_handle_selector;
  setting.style = RADIO_SELECTION_OLD_STYLE;
  setting.current_option = preferences_get_int("ZBCLI", 0);
  setting.exit_cb = menus_module_reset;
  general_radio_selection(setting);
}