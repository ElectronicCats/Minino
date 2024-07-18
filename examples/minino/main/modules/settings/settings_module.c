#include "settings_module.h"
#include "configuration.h"
#include "esp_log.h"
#include "gps_module.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "settings_module.h"

static const char* TAG = "settings_module";

void update_time_zone_options() {
  uint8_t selected_option = gps_module_get_time_zone();
  menu_screens_update_options(gps_time_zone_options, selected_option);
}

void settings_module_exit_submenu_cb() {
  screen_module_menu_t current_menu = menu_screens_get_current_menu();

  switch (current_menu) {
    case MENU_SETTINGS:
      // case MENU_SETTINGS_WIFI:
      settings_module_exit();
      break;
    default:
      break;
  }
}

void settings_module_enter_submenu_cb(screen_module_menu_t user_selection) {
  uint8_t selected_item = menu_screens_get_selected_item();
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  switch (user_selection) {
    case MENU_SETTINGS_DISPLAY:
    case MENU_SETTINGS_SOUND:
      oled_screen_clear();
      menu_screens_display_text_banner("In development");
      break;
    case MENU_SETTINGS_TIME_ZONE:
      if (menu_screens_is_configuration(user_selection)) {
        gps_module_set_time_zone(selected_item);
      }
      update_time_zone_options();
      break;
    case MENU_SETTINGS_WIFI:
      config_module_begin(MENU_SETTINGS_WIFI);
      break;
    default:
      break;
  }
}

void settings_module_begin() {
  menu_screens_register_exit_submenu_cb(settings_module_exit_submenu_cb);
  menu_screens_register_enter_submenu_cb(settings_module_enter_submenu_cb);
}

void settings_module_exit() {
  menu_screens_unregister_submenu_cbs();
}
