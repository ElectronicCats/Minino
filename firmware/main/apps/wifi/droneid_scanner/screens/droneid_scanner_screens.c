#include "droneid_scanner_screens.h"
#include "drone_id_bmps.h"
#include "esp_log.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"

#define DRONEID_SCANNER_AUTHTYPE_POS 1
#define DRONEID_SCANNER_LOCATION_POS 3
#define DRONEID_SCANNER_COUNT_FIELD  7

static bool in_details = false;
static uav_data* droneid_list[MAX_DRONEID_PACKETS];
static char* droneid_list_str[MAX_DRONEID_PACKETS] = {NULL};
static int num_drones = 0;
static int current_position = 0;
static uav_data* drone_selected;
static char* details_text[DRONEID_SCANNER_COUNT_FIELD] = {
    "Auth", NULL, "Location", NULL, NULL, NULL, NULL};

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
      current_position = 0;
      droneid_scanner_show_details();
      break;
    case BUTTON_LEFT:
      droneid_scanner_exit();
      break;
    default:
      break;
  }
}

static void droneid_scanner_details_exit_cb() {
  current_position = 0;
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

  char auth_type[18];
  char loc_lat[18];
  char loc_lon[18];
  sprintf(auth_type, "Type: %d", drone_selected->auth_type);
  sprintf(loc_lat, "Lat: %f", drone_selected->base_lat_d);
  sprintf(loc_lon, "Lon: %f", drone_selected->base_long_d);

  details_text[DRONEID_SCANNER_AUTHTYPE_POS] = malloc(strlen(auth_type) + 1);
  strcpy(details_text[DRONEID_SCANNER_AUTHTYPE_POS], auth_type);

  details_text[DRONEID_SCANNER_LOCATION_POS] = malloc(strlen(loc_lat) + 1);
  strcpy(details_text[DRONEID_SCANNER_LOCATION_POS], loc_lat);

  details_text[DRONEID_SCANNER_LOCATION_POS + 1] = malloc(strlen(loc_lon) + 1);
  strcpy(details_text[DRONEID_SCANNER_LOCATION_POS + 1], loc_lon);

  general_scrolling_text_ctx menu_details = {0};
  menu_details.banner = "Details";
  menu_details.text_arr = details_text;
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
  for (int i = 0; i < num_drones; i++) {
    oled_screen_display_bmp_text(
        drone_id_bmp_1_16x8, droneid_list_str[i], i + 1, 16, 8,
        i == current_position ? OLED_DISPLAY_INVERT : OLED_DISPLAY_NORMAL);
  }
  oled_screen_display_show();
}

void droneid_scanner_update_list(uint8_t* mac, uav_data* uas_data) {
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
    if (droneid_list_str[num_drones] != NULL) {
      free(droneid_list_str[num_drones]);
      droneid_list_str[num_drones] = NULL;
    }

    droneid_list_str[num_drones] = (char*) malloc(ODID_ID_SIZE + 1);
    snprintf(droneid_list_str[num_drones], ODID_ID_SIZE + 1, "%02x:%02x:%02x",
             mac[0], mac[1], mac[2]);

    droneid_list[num_drones] = uas_data;
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