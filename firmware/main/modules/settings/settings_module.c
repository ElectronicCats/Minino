#include "settings_module.h"
#include <string.h>
#include "coroutine.h"
#include "display_settings.h"
#include "esp_log.h"
#include "file_manager_module.h"
#include "general_radio_selection.h"
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

char* sd_card_info_2[6];
general_menu_t SD_inf = {.menu_count = 6,
                         .menu_items = sd_card_info_2,
                         .menu_level = GENERAL_MENU_MAIN};

void update_sd_card_info() {
  sd_card_mount();
  oled_screen_clear();
  modals_module_show_banner("Loading...");
  vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait for the SD card to be mounted

  if (sd_card_is_not_mounted()) {
    sd_card_info_2[0] = "";
    sd_card_info_2[1] = "No SD Card";
    sd_card_info_2[2] = "";
    sd_card_info_2[3] = "";
    sd_card_info_2[4] = "";
    sd_card_info_2[5] = "";
  } else {
    sd_card_info_t sd_info = sd_card_get_info();
    char* name_str = malloc(strlen(sd_info.name) + 1);
    sprintf(name_str, "Name: %s", sd_info.name);
    char* capacity_str = malloc(20);
    sprintf(capacity_str, "Space: %.2fGB",
            ((float) sd_info.total_space) / 1024);
    char* speed_str = malloc(25);
    sprintf(speed_str, "Speed: %.2fMHz", sd_info.speed);
    char* type_str = malloc(strlen(sd_info.type) + 1 + 6);
    sprintf(type_str, "Type: %s", sd_info.type);

    sd_card_info_2[0] = strdup("");
    sd_card_info_2[1] = strdup("SD Card Info");
    sd_card_info_2[2] = strdup(name_str);
    sd_card_info_2[3] = strdup(capacity_str);
    sd_card_info_2[4] = strdup(speed_str);
    sd_card_info_2[5] = strdup(type_str);

    free(name_str);
    free(capacity_str);
    free(speed_str);
    free(type_str);
  }

  sd_card_unmount();
  general_register_scrolling_menu(&SD_inf);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}