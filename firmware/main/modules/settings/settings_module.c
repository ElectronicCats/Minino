#include "settings_module.h"
#include <string.h>
#include "coroutine.h"
#include "display_settings.h"
#include "esp_log.h"
#include "file_manager_module.h"
#include "general_radio_selection.h"
#include "gps_module.h"
#include "menus_module.h"
#include "modals_module.h"
#include "modules/settings/wifi/wifi_settings.h"
#include "oled_screen.h"
#include "preferences.h"
#include "sd_card.h"
#include "sd_card_settings_module.h"
#include "settings_module.h"
#include "stealth_mode.h"
#include "web_file_browser_module.h"

static const char* TAG = "settings_module";

char* gps_time_zone_options_2[] = {
    "UTC-12",   "UTC-11",   "UTC-10",    "UTC-9:30", "UTC-9",    "UTC-8",
    "UTC-7",    "UTC-6",    "UTC-5",     "UTC-4",    "UTC-3:30", "UTC-3",
    "UTC-2",    "UTC-1",    "UTC+0",     "UTC+1",    "UTC+2",    "UTC+3",
    "UTC+3:30", "UTC+4",    "UTC+4:30",  "UTC+5",    "UTC+5:30", "UTC+5:45",
    "UTC+6",    "UTC+6:30", "UTC+7",     "UTC+8",    "UTC+8:45", "UTC+9",
    "UTC+9:30", "UTC+10",   "UTC+10:30", "UTC+11",   "UTC+12",   "UTC+12:45",
    "UTC+13",   "UTC+14"};

char* sd_card_info_2[6];
general_menu_t SD_inf = {.menu_count = 6,
                         .menu_items = sd_card_info_2,
                         .menu_level = GENERAL_MENU_MAIN};

void update_sd_card_info() {
  sd_card_mount();
  oled_screen_clear();
  modals_module_show_banner("Loading...");
  vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait for the SD card to be mounted
  sd_card_info_t sd_info = sd_card_get_info();

  char* name_str = malloc(strlen(sd_info.name) + 1);
  sprintf(name_str, "Name: %s", sd_info.name);
  char* capacity_str = malloc(20);
  sprintf(capacity_str, "Space: %.2fGB", ((float) sd_info.total_space) / 1024);
  char* speed_str = malloc(25);
  sprintf(speed_str, "Speed: %.2fMHz", sd_info.speed);
  char* type_str = malloc(strlen(sd_info.type) + 1 + 6);
  sprintf(type_str, "Type: %s", sd_info.type);

  sd_card_info_2[0] = "";
  sd_card_info_2[1] = "SD Card Info";
  sd_card_info_2[2] = name_str;
  sd_card_info_2[3] = capacity_str;
  sd_card_info_2[4] = speed_str;
  sd_card_info_2[5] = type_str;
  sd_card_unmount();
  general_register_scrolling_menu(&SD_inf);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}

void settings_module_time_zone() {
  general_radio_selection_menu_t time_zone;
  time_zone.banner = "Select Time Zone";
  time_zone.exit_cb = menus_module_exit_app;
  time_zone.options = gps_time_zone_options_2;
  time_zone.options_count = 38;
  time_zone.style = RADIO_SELECTION_OLD_STYLE;
  time_zone.current_option = gps_module_get_time_zone();
  time_zone.select_cb = gps_module_set_time_zone;
  general_radio_selection(time_zone);
}