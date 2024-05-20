#include "menu_screens_modules.h"
#include "bitmaps.h"
#include "esp_log.h"
#include "gps.h"
#include "leds.h"
#include "oled_screen.h"
#include "preferences.h"
#include "string.h"
#include "wifi_module.h"
#include "wifi_sniffer.h"
#include "zigbee_switch.h"

#define MAX_MENU_ITEMS_PER_SCREEN 3
#define TIME_ZONE                 (+8)    // Beijing Time
#define YEAR_BASE                 (2000)  // date in GPS starts from 2000

static const char* TAG = "menu_screens_modules";
uint8_t selected_item;
uint16_t num_items;
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

void menu_screens_begin() {
  selected_item = 0;
  previous_menu = MENU_MAIN;
  current_menu = MENU_MAIN;
  num_items = 0;
  bluetooth_devices_count = 0;
  nmea_hdl = NULL;

  menu_screens_run_tests();
  oled_screen_begin();

  // wifi_sniffer_register_cb(display_wifi_sniffer_cb);
  // wifi_sniffer_register_animation_cbs(display_wifi_sniffer_animation_start,
  //                                     display_wifi_sniffer_animation_stop);
  bluetooth_scanner_register_cb(display_bluetooth_scanner);

  // Show logo
  oled_screen_clear();

  if (preferences_get_bool("zigbee_deinit", false)) {
    current_menu = MENU_ZIGBEE_SPOOFING;
    preferences_put_bool("zigbee_deinit", false);
  } else if (preferences_get_bool("wifi_exit", false)) {
    current_menu = MENU_WIFI_APPS;
    preferences_put_bool("wifi_exit", false);
  } else {
    buzzer_play();
    oled_screen_display_bitmap(epd_bitmap_logo_1, 0, 0, 128, 64,
                               OLED_DISPLAY_NORMAL);
    buzzer_stop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  display_gps_init();
  // xTaskCreate(&display_wifi_sniffer_animation_task,
  //             "display_wifi_sniffer_animation_task", 2048, NULL, 15,
  //             &wifi_sniffer_animation_task_handle);
  // display_wifi_sniffer_animation_stop();
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
 * @brief Remove the scrolling text flag from the array
 *
 * @param items
 * @param length
 *
 * @return char**
 */
char** remove_srolling_text_flag(char** items, int length) {
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

  if (strcmp(submenu[0], VERTICAL_SCROLL_TEXT) == 0) {
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

/**
 * @brief Display the menu or the scrolling text
 *
 * @return void
 */
void menu_screens_display_menu() {
  char** items = get_menu_items();

  if (items == NULL) {
    ESP_LOGW(TAG, "Options is NULL");
    return;
  }

  if (strcmp(items[0], VERTICAL_SCROLL_TEXT) == 0) {
    char** text = remove_srolling_text_flag(items, num_items);
    display_scrolling_text(text);
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

void display_thread_cli() {
  // thread_cli_start();

  oled_screen_clear();
  oled_screen_display_text("Thread CLI      ", 0, 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text("Connect Minino", 0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("to a computer", 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("via USB and use", 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("screen command", 0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("(linux or mac)", 0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("or putty in", 0, 6, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("windows", 0, 7, OLED_DISPLAY_NORMAL);
}

void display_in_development_banner() {
  oled_screen_display_text(" In development", 0, 3, OLED_DISPLAY_NORMAL);
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

void menu_screens_exit_submenu() {
  ESP_LOGI(TAG, "Exiting submenu");
  previous_menu = prev_menu_table[current_menu];
  ESP_LOGI(TAG, "Previous: %s Current: %s", menu_list[previous_menu],
           menu_list[current_menu]);

  switch (current_menu) {
    case MENU_WIFI_ANALIZER_START:
      wifi_sniffer_stop();
      break;
    case MENU_WIFI_ANALIZER:
      wifi_sniffer_exit();
      break;
    case MENU_WIFI_ANALIZER_SUMMARY:
      wifi_analizer_summary[0] = VERTICAL_SCROLL_TEXT;
      wifi_analizer_summary[1] = "Summary";
      wifi_analizer_summary[2] = NULL;

      wifi_analizer_items[0] = "Start";
      wifi_analizer_items[1] = "Settings";
      wifi_analizer_items[2] = NULL;
      break;
    case MENU_BLUETOOTH_AIRTAGS_SCAN:
      if (bluetooth_scanner_is_active()) {
        bluetooth_scanner_stop();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait for the scanner to stop
      break;
    default:
      break;
  }

  // TODO: Store selected item history into flash
  selected_item = selected_item_history[current_menu];
  current_menu = previous_menu;
  menu_screens_display_menu();
}

void menu_screens_enter_submenu() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  screen_module_menu_t next_menu = next_menu_table[current_menu][selected_item];
  ESP_LOGI(TAG, "Previous: %s Current: %s Next: %s", menu_list[previous_menu],
           menu_list[current_menu], menu_list[next_menu]);

  switch (next_menu) {
    case MENU_WIFI_ANALIZER:
      wifi_module_analizer_begin();
      break;
    case MENU_WIFI_DEAUTH:
      wifi_module_deauth_begin();
      break;
    case MENU_WIFI_ANALIZER_START:
      oled_screen_clear();
      wifi_sniffer_start();
      break;
    case MENU_BLUETOOTH_AIRTAGS_SCAN:
      oled_screen_clear();
      bluetooth_scanner_start();
      break;
    case MENU_ZIGBEE_SWITCH:
      zigbee_switch_init();
      break;
    case MENU_THREAD_APPS:
    case MENU_MATTER_APPS:
    case MENU_ZIGBEE_LIGHT:
    case MENU_SETTINGS_DISPLAY:
    case MENU_SETTINGS_SOUND:
    case MENU_SETTINGS_SYSTEM:
      oled_screen_clear();
      display_in_development_banner();
      break;
    default:
      ESP_LOGI(TAG, "Unhandled menu: %s", menu_list[current_menu]);
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
