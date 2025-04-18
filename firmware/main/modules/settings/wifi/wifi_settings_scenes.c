#include "wifi_settings_scenes.h"

#include "general_radio_selection.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"
#include "wifi_settings.h"

#include "wifi_regions.h"

static uint8_t last_main_selection = 0;

void wifi_settings_scenes_main();
void wifi_settings_scenes_region();

typedef enum {
  APS_MANAGER_OPTION,
  WIFI_REGION_OPTOIN,
} wifi_settings_main_options_e;

static const char* main_options[] = {"APs Manager", "Wifi Region"};

static void main_handler(uint8_t selection) {
  last_main_selection = selection;
  switch (selection) {
    case APS_MANAGER_OPTION:
      wifi_settings_begin();
      break;
    case WIFI_REGION_OPTOIN:
      wifi_settings_scenes_region();
      break;
    default:
      break;
  }
}

static void main_exit() {
  last_main_selection = 0;
  menus_module_exit_app();
}

void wifi_settings_scenes_main() {
  general_submenu_menu_t main = {0};
  main.options = main_options;
  main.options_count = sizeof(main_options) / sizeof(char*);
  main.selected_option = last_main_selection;
  main.select_cb = main_handler;
  main.exit_cb = main_exit;

  general_submenu(main);
}

static const char* wifi_regions[] = {"Global", "America", "Europa", "Asia",
                                     "Japan"};

static void regions_handler(uint8_t selection) {
  preferences_put_uchar(WIFI_REGION_MEM, selection);
  wifi_regions_set_country();
}

void wifi_settings_scenes_region() {
  general_radio_selection_menu_t region = {0};
  region.banner = "Set region";
  region.options = wifi_regions;
  region.options_count = sizeof(wifi_regions) / sizeof(char*);
  region.style = RADIO_SELECTION_OLD_STYLE;
  region.current_option = preferences_get_uchar(WIFI_REGION_MEM, 0);
  region.select_cb = regions_handler;
  region.exit_cb = wifi_settings_scenes_main;

  general_radio_selection(region);
}

void wifi_settings_scenes_ssid_selection() {
  // general_radio_selection_menu_t ssid_submenu = {0};
  // ssid_submenu.banner = "Select SSID";
  // ssid_submenu.options = {};
  // ssid_submenu.options_count = sizeof(wifi_regions) / sizeof(char*);
  // ssid_submenu.style = RADIO_SELECTION_OLD_STYLE;
  // ssid_submenu.current_option = 0;
  // ssid_submenu.select_cb = regions_handler;
  // ssid_submenu.exit_cb = wifi_settings_scenes_main;

  // general_radio_selection(ssid_submenu);
}