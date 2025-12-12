#include "droneid_scanner_screens.h"
#include "drone_id_bmps.h"
#include "esp_log.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"

#define DRONEID_SCANNER_CHANNEL_POS  0
#define DRONEID_SCANNER_AUTHTYPE_POS 2
#define DRONEID_SCANNER_LOCATION_POS 4
#define DRONEID_SCANNER_COUNT_FIELD  8

static bool in_details = false;
static uav_data* droneid_list[MAX_DRONEID_PACKETS];
static int num_drones = 0;
static int current_position = 0;
static uav_data* drone_selected;
static char* details_text[DRONEID_SCANNER_COUNT_FIELD] = {
    NULL, "Auth", NULL, "Location", NULL, NULL, NULL, NULL};

static void droneid_scanner_show_list();
static void droneid_scanner_show_details();

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
      drone_selected = droneid_list[current_position];
      if (drone_selected == NULL) {
        ESP_LOGW("ERRRR", "Drone selected null");
      }
      droneid_scanner_show_details();
      current_position = 0;
      break;
    case BUTTON_LEFT:
      droneid_scanner_exit();
      break;
    default:
      break;
  }
}

static void droneid_scanner_details_exit_cb() {
  in_details = false;

  menus_module_set_app_state(true, droneid_scanner_main);

  free(details_text[DRONEID_SCANNER_AUTHTYPE_POS]);
  free(details_text[DRONEID_SCANNER_LOCATION_POS]);
  free(details_text[DRONEID_SCANNER_LOCATION_POS + 1]);

  details_text[DRONEID_SCANNER_AUTHTYPE_POS] = NULL;
  details_text[DRONEID_SCANNER_LOCATION_POS] = NULL;
  details_text[DRONEID_SCANNER_LOCATION_POS + 1] = NULL;

  droneid_scanner_show_list();
}

static void droneid_scanner_show_details() {
  in_details = true;

  char channel[18];
  char auth_type[18];
  char loc_lat[18];
  char loc_lon[18];
  sprintf(channel, "Channel: %d", drone_selected->channel);
  sprintf(auth_type, "Type: %d", drone_selected->auth_type);
  sprintf(loc_lat, "Lat: %.2f", drone_selected->lat_d);
  sprintf(loc_lon, "Lon: %.2f", drone_selected->long_d);

  details_text[DRONEID_SCANNER_CHANNEL_POS] = malloc(strlen(channel) + 1);
  strcpy(details_text[DRONEID_SCANNER_CHANNEL_POS], channel);

  details_text[DRONEID_SCANNER_AUTHTYPE_POS] = malloc(strlen(auth_type) + 1);
  strcpy(details_text[DRONEID_SCANNER_AUTHTYPE_POS], auth_type);

  details_text[DRONEID_SCANNER_LOCATION_POS] = malloc(strlen(loc_lat) + 1);
  strcpy(details_text[DRONEID_SCANNER_LOCATION_POS], loc_lat);

  details_text[DRONEID_SCANNER_LOCATION_POS + 1] = malloc(strlen(loc_lon) + 1);
  strcpy(details_text[DRONEID_SCANNER_LOCATION_POS + 1], loc_lon);

  general_scrolling_text_ctx menu_details = {0};
  menu_details.banner = "Details";
  menu_details.text_arr = (const char**) details_text;
  menu_details.text_len = DRONEID_SCANNER_COUNT_FIELD;
  menu_details.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  menu_details.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  menu_details.exit_cb = droneid_scanner_details_exit_cb;

  general_scrolling_text_array(menu_details);
}

static void droneid_scanner_show_list() {
  if (in_details) {
    return;
  }
  oled_screen_clear_buffer();
  oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);
  char title[18];
  for (int i = 0; i < num_drones; i++) {
    sprintf(title, "%02x:%02x:%02x", droneid_list[i]->mac[0],
            droneid_list[i]->mac[1], droneid_list[i]->mac[2]);
    oled_screen_display_bmp_text(
        drone_id_bmp_1_16x8, title, i + 1, 16, 8,
        i == current_position ? OLED_DISPLAY_INVERT : OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}

void droneid_scanner_update_list(uint8_t* mac, uav_data* uas_data) {
  bool found = false;
  int tmp = num_drones;
  char temp_mac_str[ODID_ID_SIZE + 1];
  char temp_dmac_str[ODID_ID_SIZE + 1];
  snprintf(temp_mac_str, sizeof(temp_mac_str), "%02x:%02x:%02x", mac[0], mac[1],
           mac[2]);
  for (int i = 0; i < num_drones; i++) {
    snprintf(temp_dmac_str, sizeof(temp_dmac_str), "%02x:%02x:%02x",
             droneid_list[i]->mac[0], droneid_list[i]->mac[1],
             droneid_list[i]->mac[2]);
    if (strcmp(temp_dmac_str, temp_mac_str) == 0) {
      found = true;
      break;
    }
  }
  if (!found) {
    if (droneid_list[num_drones] != NULL) {
      free(droneid_list[num_drones]);
    }

    droneid_list[num_drones] = (uav_data*) malloc(sizeof(uav_data));
    if (droneid_list[num_drones] != NULL) {
      memcpy(droneid_list[num_drones], uas_data, sizeof(uav_data));
    }
    num_drones = (num_drones + 1) % MAX_DRONEID_PACKETS;
  }

  if (num_drones > tmp) {
    droneid_scanner_show_list();
  }
}

void droneid_scanner_screen_main() {
  menus_module_set_app_state(true, droneid_scanner_main);

  oled_screen_clear();
  oled_screen_display_text_center("Searching for", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Drones", 2, OLED_DISPLAY_NORMAL);
}