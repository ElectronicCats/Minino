#include "droneid_scanner_screens.h"
#include "esp_log.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"

// static ODID_UAS_Data droneid_list[MAX_DRONEID_PACKETS];
static char* droneid_list_str[MAX_DRONEID_PACKETS] = {NULL};
static int num_drones = 0;

static void droneid_scanner_exit() {
  for (int i = 0; i < MAX_DRONEID_PACKETS; i++) {
    if (droneid_list_str[i] != NULL) {
      free(droneid_list_str[i]);
      droneid_list_str[i] = NULL;
    }
  }
  menus_module_exit_app();
}

static void droneid_scanner_main(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_LEFT:
      droneid_scanner_exit();
      break;
    default:
      break;
  }
}

void droneid_scanner_update_list(uint8_t* mac) {
  oled_screen_clear();
  for (int i = 0; i < num_drones; i++) {
    if (droneid_list_str[i] == NULL) {
      droneid_list_str[i] = malloc(20);
    }
    sprintf(droneid_list_str[i], "Drone %02x:%02x", mac[0], mac[1]);
    oled_screen_display_text_center(droneid_list_str[i], i,
                                    OLED_DISPLAY_NORMAL);
  }
  ESP_LOGI("DRONESCREENS", "MAC: %02x:%02x", mac[0], mac[1]);
  // droneid_list_str[num_drones] = malloc(20);
  // sprintf(droneid_list_str[num_drones], "Drone %d", num_drones);
  num_drones++;
  if (num_drones >= MAX_DRONEID_PACKETS) {
    num_drones = 0;
  }
  // general_submenu(main_menu);
}

void droneid_scanner_screen_main() {
  menus_module_set_app_state(true, droneid_scanner_main);
  for (int i = 0; i < MAX_DRONEID_PACKETS; i++) {
    droneid_list_str[i] = NULL;
  }

  oled_screen_clear();
  oled_screen_display_text_center("Searching for", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Drones", 2, OLED_DISPLAY_NORMAL);
}