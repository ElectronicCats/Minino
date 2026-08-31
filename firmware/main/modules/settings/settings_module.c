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

static char s_sd_card_info_str[6][32];
char* sd_card_info_2[6] = {s_sd_card_info_str[0], s_sd_card_info_str[1],
                           s_sd_card_info_str[2], s_sd_card_info_str[3],
                           s_sd_card_info_str[4], s_sd_card_info_str[5]};
general_menu_t SD_inf = {.menu_count = 6,
                         .menu_items = sd_card_info_2,
                         .menu_level = GENERAL_MENU_MAIN};

void update_sd_card_info() {
  sd_card_mount();
  oled_screen_clear();
  modals_module_show_banner("Loading...");
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for the SD card to be mounted

  if (sd_card_is_not_mounted()) {
    s_sd_card_info_str[0][0] = '\0';
    snprintf(s_sd_card_info_str[1], sizeof(s_sd_card_info_str[1]),
             "No SD Card");
    s_sd_card_info_str[2][0] = '\0';
    s_sd_card_info_str[3][0] = '\0';
    s_sd_card_info_str[4][0] = '\0';
    s_sd_card_info_str[5][0] = '\0';
  } else {
    sd_card_info_t sd_info = sd_card_get_info();
    s_sd_card_info_str[0][0] = '\0';
    snprintf(s_sd_card_info_str[1], sizeof(s_sd_card_info_str[1]),
             "SD Card Info");
    snprintf(s_sd_card_info_str[2], sizeof(s_sd_card_info_str[2]),
             "Name: %.16s", sd_info.name ? sd_info.name : "SD");
    snprintf(s_sd_card_info_str[3], sizeof(s_sd_card_info_str[3]),
             "Space: %.2fGB", ((float) sd_info.total_space) / 1024.0f);
    snprintf(s_sd_card_info_str[4], sizeof(s_sd_card_info_str[4]),
             "Speed: %.2fMHz", sd_info.speed);
    snprintf(s_sd_card_info_str[5], sizeof(s_sd_card_info_str[5]), "Type: %s",
             sd_info.type ? sd_info.type : "Unknown");
  }

  sd_card_unmount();
  general_register_scrolling_menu(&SD_inf);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}