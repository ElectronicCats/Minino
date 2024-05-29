#include "menu_screens_modules.h"
#include "bitmaps.h"
#include "ble_module.h"
#include "esp_log.h"
#include "gps.h"
#include "leds.h"
#include "oled_screen.h"
#include "preferences.h"
#include "string.h"
#include "wifi_module.h"
#include "wifi_sniffer.h"
#include "zigbee_module.h"
#include "zigbee_switch.h"

#include "openthread.h"
#include "radio_selector.h"

#define MAX_MENU_ITEMS_PER_SCREEN 3
#define TIME_ZONE                 (-6)    // Beijing Time
#define YEAR_BASE                 (2000)  // date in GPS starts from 2000

static const char* TAG = "menu_screens_modules";
uint8_t selected_item;
uint32_t num_items;
screen_module_menu_t previous_menu;
screen_module_menu_t current_menu;
uint8_t bluetooth_devices_count;
nmea_parser_handle_t nmea_hdl;

static app_state_t app_state = {
    .in_app = false,
    .app_handler = NULL,
};

static void gps_event_handler(void* event_handler_arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data);

esp_err_t menu_screens_test_menu_list() {
  ESP_LOGI(TAG, "Testing menus list size");
  size_t menu_list_size = sizeof(menu_list) / sizeof(menu_list[0]);
  if (menu_list_size != MENU_COUNT) {
    ESP_LOGE(TAG, "menu_list size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t menu_screens_test_menu_next_menu_table() {
  ESP_LOGI(TAG, "Testing next menu table size");
  size_t next_menu_table_size =
      sizeof(next_menu_table) / sizeof(next_menu_table[0]);
  if (next_menu_table_size != MENU_COUNT) {
    ESP_LOGE(TAG, "next_menu_table size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t menu_screens_test_prev_menu_table() {
  ESP_LOGI(TAG, "Testing previous menu table size");
  size_t prev_menu_table_size =
      sizeof(prev_menu_table) / sizeof(prev_menu_table[0]);
  if (prev_menu_table_size != MENU_COUNT) {
    ESP_LOGE(TAG, "prev_menu_table size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t menu_screens_test_menu_items() {
  ESP_LOGI(TAG, "Testing menu items size");
  size_t menu_items_size = sizeof(menu_items) / sizeof(menu_items[0]);
  if (menu_items_size != MENU_COUNT) {
    ESP_LOGE(TAG, "menu_items size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

void menu_screens_run_tests() {
  ESP_ERROR_CHECK(menu_screens_test_menu_list());
  ESP_ERROR_CHECK(menu_screens_test_menu_next_menu_table());
  ESP_ERROR_CHECK(menu_screens_test_prev_menu_table());
  ESP_ERROR_CHECK(menu_screens_test_menu_items());
}

void show_logo() {
  if (preferences_get_bool("zigbee_deinit", false)) {
    current_menu = MENU_ZIGBEE_SPOOFING;
    preferences_put_bool("zigbee_deinit", false);
  } else if (preferences_get_bool("wifi_exit", false)) {
    current_menu = MENU_WIFI_APPS;
    preferences_put_bool("wifi_exit", false);
  } else if (preferences_get_bool("thread_deinit", false)) {
    current_menu = MENU_APPLICATIONS;
    preferences_put_bool("thread_deinit", false);
  } else {
    buzzer_play();
    oled_screen_display_bitmap(epd_bitmap_logo_1, 0, 0, 128, 64,
                               OLED_DISPLAY_NORMAL);
    buzzer_stop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void menu_screens_begin() {
  selected_item = 0;
  previous_menu = MENU_MAIN;
  current_menu = MENU_MAIN;
  num_items = 0;
  bluetooth_devices_count = 0;
  nmea_hdl = NULL;

  menu_screens_run_tests();
  oled_screen_begin();
  bluetooth_scanner_register_cb(display_bluetooth_scanner);

  // Show logo
  oled_screen_clear();
  show_logo();
  // display_gps_init();
}

/**
 * @brief Add empty strings at the beginning and end of the array
 *
 * @param array
 * @param length
 *
 * @return char**
 */
char** add_empty_strings(char** array, int length) {
  char** newArray = malloc((length + 2) * sizeof(char*));

  // Add the empty string at the beginning
  newArray[0] = strdup("");

  // Copy the original array
  for (int i = 0; i < length; i++) {
    newArray[i + 1] = strdup(array[i]);
  }

  // Add the empty string at the end
  newArray[length + 1] = strdup("");

  num_items = length + 2;

  return newArray;
}

/**
 * @brief Remove the items flag
 *
 * @param items
 * @param length
 *
 * @return char**
 */
char** remove_items_flag(char** items, int length) {
  char** newArray = malloc((length - 1) * sizeof(char*));

  for (int i = 0; i < length - 1; i++) {
    newArray[i] = strdup(items[i + 1]);
    // ESP_LOGI(TAG, "Item: %s", newArray[i]);
  }
  // ESP_LOGI(TAG, "Number of items: %d", length - 1);

  num_items = length + 1;

  return newArray;
}

/**
 * @brief Check if the current menu is empty
 *
 * @return bool
 */
bool is_menu_empty(screen_module_menu_t menu) {
  return menu_items[menu][0] == NULL;
}

/**
 * @brief Check if the current menu is vertical scroll
 *
 * @return bool
 */
bool is_menu_vertical_scroll(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], VERTICAL_SCROLL_TEXT) == 0;
}

/**
 * @brief Check if the current menu is configuration
 *
 * @return bool
 */
bool is_menu_configuration(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], CONFIGURATION_MENU_ITEMS) == 0;
}

bool is_menu_question(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], QUESTION_MENU_ITEMS) == 0;
}

/**
 * @brief Get the menu items for the current menu
 *
 * @return char**
 */
char** get_menu_items() {
  num_items = 0;
  char** submenu = menu_items[current_menu];
  if (submenu != NULL) {
    while (submenu[num_items] != NULL) {
      // ESP_LOGI(TAG, "Item: %s", submenu[num_items]);
      num_items++;
    }
  }
  // ESP_LOGI(TAG, "Number of items: %d", num_items);

  if (num_items == 0) {
    return NULL;
  }

  if (is_menu_vertical_scroll(current_menu) ||
      is_menu_configuration(current_menu) || is_menu_question(current_menu)) {
    return submenu;
  }

  return add_empty_strings(menu_items[current_menu], num_items);
}

/**
 * @brief Display the menu items
 *
 * Show only 3 options at a time in the following order:
 * Page 1: Option 1
 * Page 3: Option 2 -> selected option
 * Page 5: Option 3
 *
 * @param items
 *
 * @return void
 */
void display_menu_items(char** items) {
  oled_screen_clear();
  int page = 1;
  for (int i = 0; i < 3; i++) {
    char* text = (char*) malloc(20);
    if (i == 0) {
      sprintf(text, " %s", items[i + selected_item]);
    } else if (i == 1) {
      sprintf(text, "  %s", items[i + selected_item]);
    } else {
      sprintf(text, " %s", items[i + selected_item]);
    }

    oled_screen_display_text(text, 0, page, OLED_DISPLAY_NORMAL);
    page += 2;
  }

  oled_screen_display_selected_item_box();
  oled_screen_display_show();
}

/**
 * @brief Display the scrolling text
 *
 * @param text
 *
 * @return void
 */
void display_scrolling_text(char** text) {
  uint8_t startIdx = (selected_item >= 7) ? selected_item - 6 : 0;
  selected_item = (num_items - 2 > 7 && selected_item < 6) ? 6 : selected_item;
  oled_screen_clear();
  // ESP_LOGI(TAG, "num: %d", num_items - 2);

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    // ESP_LOGI(TAG, "Text[%d]: %s", i, text[i]);
    if (i == selected_item) {
      oled_screen_display_text(
          text[i], 0, i - startIdx,
          OLED_DISPLAY_NORMAL);  // Change it to INVERT to debug
    } else {
      oled_screen_display_text(text[i], 0, i - startIdx, OLED_DISPLAY_NORMAL);
    }
  }
}

void display_configuration_items(char** items) {
  uint8_t startIdx = (selected_item >= 7) ? selected_item - 6 : 0;
  oled_screen_clear();

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    if (i == selected_item) {
      oled_screen_display_text(items[i], 0, i - startIdx, OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text(items[i], 0, i - startIdx, OLED_DISPLAY_NORMAL);
    }
  }
}

void display_question_items(char** items) {
  oled_screen_clear();
  uint8_t page_offset = 4;
  for (uint8_t i = 0; i < num_items - 2; i++) {
    if (i == selected_item) {
      oled_screen_display_text_center(items[i], i + page_offset,
                                      OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text_center(items[i], i + page_offset,
                                      OLED_DISPLAY_NORMAL);
    }
  }
}

/**
 * @brief Display the menu items
 *
 * @return void
 */
void menu_screens_display_menu() {
  char** items = get_menu_items();

  if (items == NULL) {
    ESP_LOGW(TAG, "Options is NULL");
    return;
  }

  if (is_menu_vertical_scroll(current_menu)) {
    char** text = remove_items_flag(items, num_items);
    display_scrolling_text(text);
  } else if (is_menu_configuration(current_menu)) {
    char** new_items = remove_items_flag(items, num_items);
    display_configuration_items(new_items);
  } else if (is_menu_question(current_menu)) {
    char** new_items = remove_items_flag(items, num_items);
    display_question_items(new_items);
  } else {
    display_menu_items(items);
  }
}

void display_bluetooth_scanner(bluetooth_scanner_record_t record) {
  static bool airtag_detected = false;
  oled_screen_display_text("Airtags Scanner", 0, 0, OLED_DISPLAY_INVERT);
  uint8_t x = 0;
  uint8_t y = 2;
  oled_screen_clear_line(x, y, OLED_DISPLAY_NORMAL);

  if (record.has_finished && !airtag_detected) {
    oled_screen_display_text("    Scanning", 0, 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_text("    Finished", 0, 4, OLED_DISPLAY_NORMAL);
    return;
  }

  if (!record.is_airtag) {
    airtag_detected = false;
    bluetooth_devices_count++;
    char* device_count_str = (char*) malloc(16);
    sprintf(device_count_str, "Devices=%d", record.count);
    oled_screen_display_text(device_count_str, 0, 2, OLED_DISPLAY_NORMAL);
    return;
  }

  airtag_detected = true;
  char* name_str = (char*) malloc(50);
  char* addr_str1 = (char*) malloc(14);
  char* addr_str2 = (char*) malloc(14);
  char* rssi_str = (char*) malloc(16);

  sprintf(name_str, "%s", record.name);
  sprintf(addr_str1, "MAC= %02X:%02X:%02X", record.mac[5], record.mac[4],
          record.mac[3]);
  sprintf(addr_str2, "     %02X:%02X:%02X", record.mac[2], record.mac[1],
          record.mac[0]);
  sprintf(rssi_str, "RSSI=%d", record.rssi);

  oled_screen_display_text(name_str, 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(addr_str1, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(addr_str2, 0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(rssi_str, 0, 5, OLED_DISPLAY_NORMAL);
}

void display_thread_broadcast() {
  radio_selector_enable_thread();
  openthread_init();

  oled_screen_clear();
  oled_screen_display_text("Waiting messages...  ", 0, 0, OLED_DISPLAY_INVERT);
}

void menu_screens_display_in_development_banner() {
  oled_screen_display_text(" In development", 0, 3, OLED_DISPLAY_NORMAL);
}

void menu_screens_display_loading_banner() {
  oled_screen_display_text("   Loading...", 0, 3, OLED_DISPLAY_NORMAL);
}

void menu_screens_update_options(char* options[], uint8_t selected_option) {
  uint8_t i = 0;
  uint32_t menu_length = menu_screens_get_menu_length(options);

  for (i = 1; i < menu_length; i++) {
    char* prev_item = options[i];
    // ESP_LOGI(TAG, "Prev item: %s", prev_item);
    char* new_item = malloc(strlen(prev_item) + 5);
    char* start_of_number = strchr(prev_item, ']') + 2;
    if (i == selected_option) {
      snprintf(new_item, strlen(prev_item) + 5, "[x] %s", start_of_number);
      options[i] = new_item;
    } else {
      snprintf(new_item, strlen(prev_item) + 5, "[ ] %s", start_of_number);
      options[i] = new_item;
    }
    // ESP_LOGI(TAG, "New item: %s", options[i]);
  }
  options[i] = NULL;
}

void display_gps_init() {
  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_hdl = gps_init(&config);
  /* register event handler for NMEA parser library */
  gps_add_handler(nmea_hdl, gps_event_handler, NULL);
}

void display_gps_deinit() {
  /* unregister event handler */
  gps_remove_handler(nmea_hdl, gps_event_handler);
  /* deinit NMEA parser library */
  gps_deinit(nmea_hdl);
}

/**
 * @brief GPS Event Handler
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to ESP_NMEA_EVENT
 * @param event_id event id
 * @param event_data event specific arguments
 */
static void gps_event_handler(void* event_handler_arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data) {
  if (current_menu != MENU_GPS_DATE_TIME && current_menu != MENU_GPS_LOCATION) {
    return;
  }

  gps_t* gps = NULL;
  switch (event_id) {
    case GPS_UPDATE:
      gps = (gps_t*) event_data;
      /* print information parsed from GPS statements */
      ESP_LOGI(TAG,
               "%d/%d/%d %d:%d:%d => \r\n"
               "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
               "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
               "\t\t\t\t\t\taltitude   = %.02fm\r\n"
               "\t\t\t\t\t\tspeed      = %fm/s",
               gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
               gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
               gps->latitude, gps->longitude, gps->altitude, gps->speed);

      if (current_menu == MENU_GPS_DATE_TIME) {
        char* date_str = (char*) malloc(20);
        char* time_str = (char*) malloc(20);

        sprintf(date_str, "Date: %d/%d/%d", gps->date.year + YEAR_BASE,
                gps->date.month, gps->date.day);
        // TODO: fix time +24
        sprintf(time_str, "Time: %d:%d:%d", gps->tim.hour + TIME_ZONE,
                gps->tim.minute, gps->tim.second);

        oled_screen_clear();
        oled_screen_display_text("GPS Date/Time", 0, 0, OLED_DISPLAY_INVERT);
        // TODO: refresh only the date and time
        oled_screen_display_text(date_str, 0, 2, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(time_str, 0, 3, OLED_DISPLAY_NORMAL);
      } else if (current_menu == MENU_GPS_LOCATION) {
        char* latitude_str = (char*) malloc(22);
        char* longitude_str = (char*) malloc(22);
        char* altitude_str = (char*) malloc(22);
        char* speed_str = (char*) malloc(22);

        sprintf(latitude_str, "Latitude: %.05f°N", gps->latitude);
        sprintf(longitude_str, "Longitude: %.05f°E", gps->longitude);
        sprintf(altitude_str, "Altitude: %.02fm", gps->altitude);
        sprintf(speed_str, "Speed: %fm/s", gps->speed);

        oled_screen_clear();
        oled_screen_display_text("GPS Location", 0, 0, OLED_DISPLAY_INVERT);
        oled_screen_display_text(latitude_str, 0, 2, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(longitude_str, 0, 3, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(altitude_str, 0, 4, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(speed_str, 0, 5, OLED_DISPLAY_NORMAL);
      }
      break;
    case GPS_UNKNOWN:
      /* print unknown statements */
      ESP_LOGW(TAG, "Unknown statement:%s", (char*) event_data);
      break;
    default:
      break;
  }
}

app_state_t menu_screens_get_app_state() {
  return app_state;
}

void menu_screens_set_app_state(bool in_app, app_handler_t app_handler) {
  app_state.in_app = in_app;
  app_state.app_handler = app_handler;
}

screen_module_menu_t menu_screens_get_current_menu() {
  return current_menu;
}

uint32_t menu_screens_get_menu_length(char* menu[]) {
  uint32_t num_items = 0;
  if (menu != NULL) {
    while (menu[num_items] != NULL) {
      ESP_LOGI(TAG, "Item: %s", menu[num_items]);
      num_items++;
    }
  }
  return num_items;
}

void menu_screens_exit_submenu() {
  ESP_LOGI(TAG, "Exiting submenu");
  previous_menu = prev_menu_table[current_menu];
  ESP_LOGI(TAG, "Previous: %s Current: %s", menu_list[previous_menu],
           menu_list[current_menu]);

  switch (current_menu) {
    case MENU_WIFI_ANALIZER_RUN:
      wifi_sniffer_stop();
      break;
    case MENU_WIFI_ANALIZER_ASK_SUMMARY:
      oled_screen_clear();
      wifi_sniffer_start();
      break;
    case MENU_WIFI_ANALIZER_SUMMARY:
      wifi_sniffer_close_file();
      break;
    case MENU_WIFI_ANALIZER:
      oled_screen_clear();
      menu_screens_display_loading_banner();
      wifi_sniffer_exit();
      break;
    case MENU_BLUETOOTH_AIRTAGS_SCAN:
      if (bluetooth_scanner_is_active()) {
        bluetooth_scanner_stop();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait for the scanner to stop
      break;
    case MENU_THREAD_APPS:
      openthread_factory_reset();
      break;
    default:
      break;
  }

  // TODO: Store selected item history into flash
  selected_item = selected_item_history[current_menu];
  current_menu = previous_menu;
  menu_screens_display_menu();
}
void module_keyboard_update_state(
    bool in_app,
    void (*app_handler)(button_event_t button_pressed)) {
  app_state.in_app = in_app;
  app_state.app_handler = app_handler;
}

void menu_screens_enter_submenu() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  screen_module_menu_t next_menu = MENU_MAIN;
  bool update_configuration = false;

  if (is_menu_empty(current_menu)) {
    ESP_LOGW(TAG, "Empty menu");
    return;
  } else if (is_menu_vertical_scroll(current_menu)) {
    selected_item = 0;  // Avoid selecting invalid item
    next_menu = current_menu;
  } else if (is_menu_configuration(current_menu)) {
    next_menu = current_menu;
  } else {
    next_menu = next_menu_table[current_menu][selected_item];
  }

  ESP_LOGI(TAG, "Previous: %s Current: %s", menu_list[current_menu],
           menu_list[next_menu]);

  // User is selecting a configuration item
  if (is_menu_configuration(next_menu) && current_menu == next_menu) {
    update_configuration = true;
  }

  // next_menu is the user selection
  switch (next_menu) {
    case MENU_WIFI_ANALIZER:
      wifi_module_analizer_begin();
      break;
    case MENU_WIFI_DEAUTH:
      wifi_module_deauth_begin();
      break;
    case MENU_WIFI_ANALIZER_RUN:
      oled_screen_clear();
      wifi_sniffer_start();
      break;
    case MENU_WIFI_ANALIZER_SUMMARY:
      wifi_sniffer_load_summary();
      break;
    case MENU_WIFI_ANALIZER_CHANNEL:
      if (update_configuration) {
        wifi_sniffer_set_channel(selected_item + 1);
      }
      wifi_module_update_channel_options();
      break;
    case MENU_WIFI_ANALIZER_DESTINATION:
      if (update_configuration) {
        if (selected_item == WIFI_SNIFFER_DESTINATION_SD) {
          wifi_sniffer_set_destination_sd();
        } else {
          wifi_sniffer_set_destination_internal();
        }
      }
      wifi_module_update_destination_options();
      break;
    case MENU_BLUETOOTH_AIRTAGS_SCAN:
      oled_screen_clear();
      bluetooth_scanner_start();
      break;
    case MENU_BLUETOOTH_TRAKERS_SCAN:
      ble_module_begin(MENU_BLUETOOTH_TRAKERS_SCAN);
      break;
    case MENU_BLUETOOTH_SPAM:
      ble_module_begin(MENU_BLUETOOTH_SPAM);
      break;
    case MENU_ZIGBEE_SWITCH:
      radio_selector_disable_thread();
      zigbee_switch_init();
      break;
    case MENU_ZIGBEE_SNIFFER:
      zigbee_module_begin(MENU_ZIGBEE_SNIFFER);
      break;
    case MENU_THREAD_BROADCAST:
    case MENU_THREAD_APPS:
      display_thread_broadcast();
      break;
    case MENU_MATTER_APPS:
    case MENU_ZIGBEE_LIGHT:
    case MENU_SETTINGS_DISPLAY:
    case MENU_SETTINGS_SOUND:
    case MENU_SETTINGS_SYSTEM:
      oled_screen_clear();
      menu_screens_display_in_development_banner();
      break;
    default:
      ESP_LOGI(TAG, "Unhandled menu: %s", menu_list[next_menu]);
      break;
  }

  if (current_menu != next_menu) {
    selected_item_history[next_menu] = selected_item;
    selected_item = 0;
  }
  current_menu = next_menu;

  if (!app_state.in_app) {
    menu_screens_display_menu();
  }
}

void menu_screens_ingrement_selected_item() {
  selected_item = (selected_item == num_items - MAX_MENU_ITEMS_PER_SCREEN)
                      ? selected_item
                      : selected_item + 1;
  menu_screens_display_menu();
}

void menu_screens_decrement_selected_item() {
  selected_item = (selected_item == 0) ? 0 : selected_item - 1;
  menu_screens_display_menu();
}
