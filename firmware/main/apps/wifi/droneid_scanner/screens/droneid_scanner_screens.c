#include "droneid_scanner_screens.h"
#include "drone_id_bmps.h"
#include "esp_log.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"

// static ODID_UAS_Data droneid_list[MAX_DRONEID_PACKETS];
static char* droneid_list_str[MAX_DRONEID_PACKETS] = {NULL};
static int num_drones = 0;
static int current_position = 0;

static void droneid_scanner_show_list();

static void droneid_scanner_exit() {
  menus_module_restart();
}

static void droneid_scanner_main(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      current_position = (current_position - 1 + num_drones) % num_drones;
      droneid_scanner_show_list();
      break;
    case BUTTON_DOWN:
      current_position = (current_position + 1) % num_drones;
      droneid_scanner_show_list();
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

static void droneid_scanner_show_list() {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);
  for (int i = 0; i < num_drones; i++) {
    oled_screen_display_bmp_text(
        drone_id_bmp_1_16x8, droneid_list_str[i], i + 1, 16, 8,
        i == current_position ? OLED_DISPLAY_INVERT : OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}

void droneid_scanner_update_list(uint8_t* mac) {
  bool found = false;
  int tmp = num_drones;
  char temp_mac_str[ODID_ID_SIZE + 1];
  snprintf(temp_mac_str, sizeof(temp_mac_str), "%02x:%02x:%02x", mac[0], mac[1],
           mac[2]);
  for (int i = 0; i < num_drones; i++) {
    if (strcmp(droneid_list_str[i], temp_mac_str) == 0) {
      found = true;
      break;
    }
  }
  if (!found) {
    ESP_LOGI("DRONESCREENS", "MAC: %02x:%02x:%02x", mac[0], mac[1], mac[2]);
    if (droneid_list_str[num_drones] != NULL) {
      free(droneid_list_str[num_drones]);
      droneid_list_str[num_drones] = NULL;
    }

    droneid_list_str[num_drones] = (char*) malloc(ODID_ID_SIZE + 1);
    snprintf(droneid_list_str[num_drones], ODID_ID_SIZE + 1, "%02x:%02x:%02x",
             mac[0], mac[1], mac[2]);
    num_drones = (num_drones + 1) % MAX_DRONEID_PACKETS;
  }

  if (num_drones > tmp) {
    droneid_scanner_show_list();
  }
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