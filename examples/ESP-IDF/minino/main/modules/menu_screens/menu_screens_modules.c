#include "menu_screens_modules.h"
#include "bitmaps.h"
#include "esp_log.h"
#include "gps.h"
#include "leds.h"
#include "preferences.h"
#include "string.h"
#include "zigbee_light.h"
#include "zigbee_switch.h"

#define MAX_MENU_ITEMS_PER_SCREEN 3
#define TIME_ZONE                 (+8)    // Beijing Time
#define YEAR_BASE                 (2000)  // date in GPS starts from 2000

static const char* TAG = "menu_screens_modules";
SH1106_t dev;
uint8_t selected_item;
Layer previous_layer;
Layer current_layer;
int num_items;
uint8_t bluetooth_devices_count;
nmea_parser_handle_t nmea_hdl;
TaskHandle_t wifi_sniffer_task_handle = NULL;

static app_state_t app_state = {
    .in_app = false,
    .app_handler = NULL,
};

// Function prototypes
void handle_main_selection();
void handle_applications_selection();
void handle_settings_selection();
void handle_about_selection();
void handle_wifi_apps_selection();
void handle_wifi_sniffer_selection();
void handle_bluetooth_apps_selection();
void handle_zigbee_apps_selection();
void handle_zigbee_spoofing_selection();
void handle_zigbee_switch_selection();
void handle_thread_apps_selection();
void handle_gps_selection();

static void gps_event_handler(void* event_handler_arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data);

void menu_screens_init() {
  selected_item = 0;
  previous_layer = LAYER_MAIN_MENU;
  current_layer = LAYER_MAIN_MENU;
  num_items = 0;
  bluetooth_devices_count = 0;
  nmea_hdl = NULL;

#if CONFIG_I2C_INTERFACE
  ESP_LOGI(TAG, "INTERFACE is i2c");
  ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
  ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
  ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
  i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
  ESP_LOGI(TAG, "INTERFACE is SPI");
  ESP_LOGI(TAG, "CONFIG_MOSI_GPIO=%d", CONFIG_MOSI_GPIO);
  ESP_LOGI(TAG, "CONFIG_SCLK_GPIO=%d", CONFIG_SCLK_GPIO);
  ESP_LOGI(TAG, "CONFIG_CS_GPIO=%d", CONFIG_CS_GPIO);
  ESP_LOGI(TAG, "CONFIG_DC_GPIO=%d", CONFIG_DC_GPIO);
  ESP_LOGI(TAG, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
  spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO,
                  CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
  dev._flip = true;
  ESP_LOGW(TAG, "Flip upside down");
#endif

#if CONFIG_SH1106_128x64
  ESP_LOGI(TAG, "Panel is 128x64");
  sh1106_init(&dev, 128, 64);
#endif  // CONFIG_SH1106_128x64
#if CONFIG_SH1106_128x32
  ESP_LOGI(TAG, "Panel is 128x32");
  sh1106_init(&dev, 128, 32);
#endif  // CONFIG_SH1106_128x32

  wifi_sniffer_register_cb(display_wifi_sniffer_cb);
  wifi_sniffer_register_animation_cbs(display_wifi_sniffer_animation_start,
                                      display_wifi_sniffer_animation_stop);
  bluetooth_scanner_register_cb(display_bluetooth_scanner);

  // Show logo
  display_clear();

  if (preferences_get_bool("zigbee_deinit", false)) {
    current_layer = LAYER_ZIGBEE_SPOOFING;
    preferences_put_bool("zigbee_deinit", false);
  } else {
    leds_on();  // Indicate that the system is booting
    buzzer_play();
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_logo_1, 128, 64, NO_INVERT);
    buzzer_stop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  display_menu();
  display_gps_init();
  xTaskCreate(&display_wifi_sniffer_animation_task,
              "display_wifi_sniffer_animation_task", 2048, NULL, 15,
              &wifi_sniffer_task_handle);
  display_wifi_sniffer_animation_stop();
}

void display_clear() {
  sh1106_clear_screen(&dev, false);
}

void display_show() {
  sh1106_show_buffer(&dev);
}

/// @brief Display text on the screen
/// @param text
/// @param text_size
/// @param page
/// @param invert
void display_text(const char* text, int x, int page, int invert) {
  sh1106_display_text(&dev, page, text, x, invert);
}

/// @brief Clear a line on the screen
/// @param page
/// @param invert
void display_clear_line(int x, int page, int invert) {
  // sh1106_clear_line(&dev, x, page, invert);
  sh1106_bitmaps(&dev, x, page * 8, epd_bitmap_clear_line, 128 - x, 8, invert);
}

/// @brief Display a bitmap on the screen
/// @param bitmap
/// @param x
/// @param y
/// @param width
/// @param height
/// @param invert
void display_bitmap(const uint8_t* bitmap,
                    int x,
                    int y,
                    int width,
                    int height,
                    int invert) {
  sh1106_bitmaps(&dev, x, y, bitmap, width, height, invert);
}

/// @brief Display a box around the selected item
void display_selected_item_box() {
  sh1106_draw_custom_box(&dev);
}

/// @brief Add empty strings at the beginning and end of the array
/// @param array
/// @param length
/// @return Returns a new array with empty strings at the beginning and end
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

/// @brief Remove the scrolling text flag from the array
/// @param items
/// @param length
/// @return Returns a new array without the scrolling text flag
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

char** get_menu_items() {
  num_items = 0;
  char** submenu = menu_items[current_layer];
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

  if (strcmp(submenu[0], SCROLLING_TEXT) == 0) {
    return submenu;
  }

  return add_empty_strings(menu_items[current_layer], num_items);
}

void display_menu_items(char** items) {
  // Show only 3 options at a time in the following order:
  // Page 1: Option 1
  // Page 3: Option 2 -> selected option
  // Page 5: Option 3

  display_clear();
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

    display_text(text, 0, page, NO_INVERT);
    page += 2;
  }

  display_selected_item_box();
}

void display_scrolling_text(char** text) {
  uint8_t startIdx = (selected_item >= 7) ? selected_item - 6 : 0;
  selected_item = (num_items - 2 > 7 && selected_item < 6) ? 6 : selected_item;
  display_clear();
  // ESP_LOGI(TAG, "num: %d", num_items - 2);

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    // ESP_LOGI(TAG, "Text[%d]: %s", i, text[i]);
    if (i == selected_item) {
      display_text(text[i], 0, i - startIdx,
                   NO_INVERT);  // Change it to INVERT to debug
    } else {
      display_text(text[i], 0, i - startIdx, NO_INVERT);
    }
  }
}

void display_menu() {
  char** items = get_menu_items();

  if (items == NULL) {
    ESP_LOGW(TAG, "Options is NULL");
    return;
  }

  if (strcmp(items[0], SCROLLING_TEXT) == 0) {
    char** text = remove_srolling_text_flag(items, num_items);
    display_scrolling_text(text);
  } else {
    display_menu_items(items);
  }
}

void display_wifi_sniffer_animation_task(void* pvParameter) {
  while (true) {
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_wifi_loading_1, 64, 64, NO_INVERT);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_wifi_loading_2, 64, 64, NO_INVERT);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_wifi_loading_3, 64, 64, NO_INVERT);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    sh1106_bitmaps(&dev, 0, 0, epd_bitmap_wifi_loading_4, 64, 64, NO_INVERT);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void display_wifi_sniffer_animation_start() {
  vTaskResume(wifi_sniffer_task_handle);
}

void display_wifi_sniffer_animation_stop() {
  vTaskSuspend(wifi_sniffer_task_handle);
  // display_text("Timeout!", 64, 6, INVERT);
}

void display_wifi_sniffer_cb(sniffer_runtime_t* sniffer) {
  if (sniffer->is_running) {
    const char* packets_str = malloc(16);
    const char* channel_str = malloc(16);

    sprintf(packets_str, "%ld", sniffer->sniffed_packets);
    sprintf(channel_str, "%ld", sniffer->channel);

    display_clear_line(64, 1, NO_INVERT);

    display_text("Packets", 64, 0, INVERT);
    display_text(packets_str, 64, 1, INVERT);
    display_text("Channel", 64, 3, INVERT);
    display_text(channel_str, 64, 4, INVERT);
  } else {
    ESP_LOGI(TAG, "sniffer task stopped");
  }
}

void display_bluetooth_scanner(bluetooth_scanner_record_t record) {
  static bool airtag_detected = false;
  display_text("Airtags Scanner", 0, 0, INVERT);
  uint8_t x = 0;
  uint8_t y = 2;
  display_clear_line(x, y, NO_INVERT);

  if (record.has_finished && !airtag_detected) {
    display_text("    Scanning", 0, 3, NO_INVERT);
    display_text("    Finished", 0, 4, NO_INVERT);
    return;
  }

  if (!record.is_airtag) {
    airtag_detected = false;
    bluetooth_devices_count++;
    char* device_count_str = (char*) malloc(16);
    sprintf(device_count_str, "Devices=%d", record.count);
    display_text(device_count_str, 0, 2, NO_INVERT);
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

  display_text(name_str, 0, 2, NO_INVERT);
  display_text(addr_str1, 0, 3, NO_INVERT);
  display_text(addr_str2, 0, 4, NO_INVERT);
  display_text(rssi_str, 0, 5, NO_INVERT);
}

void display_thread_cli() {
  // thread_cli_start();

  display_clear();
  display_text("Thread CLI      ", 0, 0, INVERT);
  display_text("Connect Minino", 0, 1, NO_INVERT);
  display_text("to a computer", 0, 2, NO_INVERT);
  display_text("via USB and use", 0, 3, NO_INVERT);
  display_text("screen command", 0, 4, NO_INVERT);
  display_text("(linux or mac)", 0, 5, NO_INVERT);
  display_text("or putty in", 0, 6, NO_INVERT);
  display_text("windows", 0, 7, NO_INVERT);
  display_show();
}

void display_in_development_banner() {
  display_text(" In development", 0, 3, NO_INVERT);
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
  if (current_layer != LAYER_GPS_DATE_TIME &&
      current_layer != LAYER_GPS_LOCATION) {
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

      if (current_layer == LAYER_GPS_DATE_TIME) {
        char* date_str = (char*) malloc(20);
        char* time_str = (char*) malloc(20);

        sprintf(date_str, "Date: %d/%d/%d", gps->date.year + YEAR_BASE,
                gps->date.month, gps->date.day);
        // TODO: fix time +24
        sprintf(time_str, "Time: %d:%d:%d", gps->tim.hour + TIME_ZONE,
                gps->tim.minute, gps->tim.second);

        display_clear();
        display_text("GPS Date/Time", 0, 0, INVERT);
        // TODO: refresh only the date and time
        display_text(date_str, 0, 2, NO_INVERT);
        display_text(time_str, 0, 3, NO_INVERT);
      } else if (current_layer == LAYER_GPS_LOCATION) {
        char* latitude_str = (char*) malloc(22);
        char* longitude_str = (char*) malloc(22);
        char* altitude_str = (char*) malloc(22);
        char* speed_str = (char*) malloc(22);

        sprintf(latitude_str, "Latitude: %.05f°N", gps->latitude);
        sprintf(longitude_str, "Longitude: %.05f°E", gps->longitude);
        sprintf(altitude_str, "Altitude: %.02fm", gps->altitude);
        sprintf(speed_str, "Speed: %fm/s", gps->speed);

        display_clear();
        display_text("GPS Location", 0, 0, INVERT);
        display_text(latitude_str, 0, 2, NO_INVERT);
        display_text(longitude_str, 0, 3, NO_INVERT);
        display_text(altitude_str, 0, 4, NO_INVERT);
        display_text(speed_str, 0, 5, NO_INVERT);
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

void menu_screens_set_app_state(
    bool in_app,
    void (*app_handler)(button_event_t button_pressed)) {
  app_state.in_app = in_app;
  app_state.app_handler = app_handler;
}

Layer screen_module_get_current_layer(void) {
  return current_layer;
}

void menu_screens_exit_submenu() {
  ESP_LOGI(TAG, "Exiting submenu");
  ESP_LOGI(TAG, "Previous layer: %d Current: %d", previous_layer,
           current_layer);

  switch (current_layer) {
    case LAYER_WIFI_SNIFFER_START:
      wifi_sniffer_stop();
      break;
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      if (bluetooth_scanner_is_active()) {
        bluetooth_scanner_stop();
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);  // Wait for the scanner to stop
      break;
    case LAYER_THREAD_CLI:
      // thread_cli_stop();
      break;
    default:
      break;
  }

  current_layer = previous_layer;
  selected_item = 0;
  display_menu();
}

void menu_screens_enter_submenu() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  switch (current_layer) {
    case LAYER_MAIN_MENU:
      handle_main_selection();
      break;
    case LAYER_APPLICATIONS:
      handle_applications_selection();
      break;
    case LAYER_SETTINGS:
      handle_settings_selection();
      break;
    case LAYER_ABOUT:
      handle_about_selection();
      break;
    case LAYER_WIFI_APPS:
      handle_wifi_apps_selection();
      break;
    case LAYER_BLUETOOTH_APPS:
      handle_bluetooth_apps_selection();
      break;
    case LAYER_ZIGBEE_APPS:
      handle_zigbee_apps_selection();
      break;
    case LAYER_THREAD_APPS:
      handle_thread_apps_selection();
      break;
    case LAYER_MATTER_APPS:
      break;
    case LAYER_GPS:
      handle_gps_selection();
      break;
    case LAYER_WIFI_SNIFFER:
      handle_wifi_sniffer_selection();
      break;
    case LAYER_ZIGBEE_SPOOFING:
      handle_zigbee_spoofing_selection();
      break;
    case LAYER_ZIGBEE_SWITCH:
      handle_zigbee_switch_selection();
      break;
    default:
      ESP_LOGE(TAG, "Invalid layer");
      break;
  }

  selected_item = 0;
  if (!app_state.in_app) {
    display_menu();
  }
}

void menu_screens_ingrement_selected_item() {
  selected_item = (selected_item == num_items - MAX_MENU_ITEMS_PER_SCREEN)
                      ? selected_item
                      : selected_item + 1;
  display_menu();
}

void menu_screens_decrement_selected_item() {
  selected_item = (selected_item == 0) ? 0 : selected_item - 1;
  display_menu();
}

void menu_screens_update_previous_layer() {
  switch (current_layer) {
    case LAYER_MAIN_MENU:
    case LAYER_APPLICATIONS:
    case LAYER_SETTINGS:
    case LAYER_ABOUT:
      previous_layer = LAYER_MAIN_MENU;
      break;
    case LAYER_WIFI_APPS:
    case LAYER_BLUETOOTH_APPS:
    case LAYER_ZIGBEE_APPS:
    case LAYER_THREAD_APPS:
    case LAYER_MATTER_APPS:
    case LAYER_GPS:
      previous_layer = LAYER_APPLICATIONS;
      break;
    case LAYER_ABOUT_VERSION:
    case LAYER_ABOUT_LICENSE:
    case LAYER_ABOUT_CREDITS:
    case LAYER_ABOUT_LEGAL:
      previous_layer = LAYER_ABOUT;
      break;
    case LAYER_SETTINGS_DISPLAY:
    case LAYER_SETTINGS_SOUND:
    case LAYER_SETTINGS_SYSTEM:
      previous_layer = LAYER_SETTINGS;
      break;
    /* WiFi apps */
    case LAYER_WIFI_SNIFFER:
      previous_layer = LAYER_WIFI_APPS;
      break;
    /* WiFi sniffer apps */
    case LAYER_WIFI_SNIFFER_START:
    case LAYER_WIFI_SNIFFER_SETTINGS:
      previous_layer = LAYER_WIFI_SNIFFER;
      break;
    /* Bluetooth apps */
    case LAYER_BLUETOOTH_AIRTAGS_SCAN:
      previous_layer = LAYER_BLUETOOTH_APPS;
      break;
    case LAYER_ZIGBEE_SPOOFING:
      previous_layer = LAYER_ZIGBEE_APPS;
      break;
    case LAYER_ZIGBEE_SWITCH:
    case LAYER_ZIGBEE_LIGHT:
      previous_layer = LAYER_ZIGBEE_SPOOFING;
      break;
    /* GPS apps */
    case LAYER_GPS_DATE_TIME:
    case LAYER_GPS_LOCATION:
      previous_layer = LAYER_GPS;
      break;
    default:
      ESP_LOGE(TAG, "Unable to update previous layer, current layer: %d",
               current_layer);
      break;
  }
}

void handle_main_selection() {
  switch (selected_item) {
    case MAIN_MENU_APPLICATIONS:
      current_layer = LAYER_APPLICATIONS;
      break;
    case MAIN_MENU_SETTINGS:
      current_layer = LAYER_SETTINGS;
      break;
    case MAIN_MENU_ABOUT:
      current_layer = LAYER_ABOUT;
      break;
  }
}

void handle_applications_selection() {
  switch (selected_item) {
    case APPLICATIONS_MENU_WIFI:
      current_layer = LAYER_WIFI_APPS;
      break;
    case APPLICATIONS_MENU_BLUETOOTH:
      current_layer = LAYER_BLUETOOTH_APPS;
      break;
    case APPLICATIONS_MENU_ZIGBEE:
      current_layer = LAYER_ZIGBEE_APPS;
      break;
    case APPLICATIONS_MENU_THREAD:
      current_layer = LAYER_THREAD_APPS;
      break;
    case APPLICATIONS_MENU_MATTER:
      current_layer = LAYER_MATTER_APPS;
      display_clear();
      display_in_development_banner();
      break;
    case APPLICATIONS_MENU_GPS:
      current_layer = LAYER_GPS;
      break;
  }
}

void handle_settings_selection() {
  switch (selected_item) {
    case SETTINGS_MENU_DISPLAY:
      current_layer = LAYER_SETTINGS_DISPLAY;
      display_clear();
      display_in_development_banner();
      break;
    case SETTINGS_MENU_SOUND:
      current_layer = LAYER_SETTINGS_SOUND;
      display_clear();
      display_in_development_banner();
      break;
    case SETTINGS_MENU_SYSTEM:
      current_layer = LAYER_SETTINGS_SYSTEM;
      display_clear();
      display_in_development_banner();
      break;
  }
}

void handle_about_selection() {
  switch (selected_item) {
    case ABOUT_MENU_VERSION:
      current_layer = LAYER_ABOUT_VERSION;
      break;
    case ABOUT_MENU_LICENSE:
      current_layer = LAYER_ABOUT_LICENSE;
      break;
    case ABOUT_MENU_CREDITS:
      current_layer = LAYER_ABOUT_CREDITS;
      break;
    case ABOUT_MENU_LEGAL:
      current_layer = LAYER_ABOUT_LEGAL;
      break;
  }
}

void handle_wifi_apps_selection() {
  switch (selected_item) {
    case WIFI_MENU_SNIFFER:
      current_layer = LAYER_WIFI_SNIFFER;
      break;
  }
}

void handle_wifi_sniffer_selection() {
  switch (selected_item) {
    case WIFI_SNIFFER_START:
      current_layer = LAYER_WIFI_SNIFFER_START;
      display_clear();
      wifi_sniffer_start();
      break;
    case WIFI_SNIFFER_SETTINGS:
      current_layer = LAYER_WIFI_SNIFFER_SETTINGS;
      break;
    default:
      ESP_LOGE(TAG, "Invalid item: %d", selected_item);
      break;
  }
}

void handle_bluetooth_apps_selection() {
  switch (selected_item) {
    case BLUETOOTH_MENU_AIRTAGS_SCAN:
      current_layer = LAYER_BLUETOOTH_AIRTAGS_SCAN;
      display_clear();
      bluetooth_scanner_start();
      break;
  }
}

void handle_zigbee_apps_selection() {
  switch (selected_item) {
    case ZIGBEE_MENU_SPOOFING:
      current_layer = LAYER_ZIGBEE_SPOOFING;
      break;
  }
}

void handle_zigbee_spoofing_selection() {
  switch (selected_item) {
    case ZIGBEE_SPOOFING_SWITCH:
      current_layer = LAYER_ZIGBEE_SWITCH;
      zigbee_switch_init();
      break;
    case ZIGBEE_SPOOFING_LIGHT:
      current_layer = LAYER_ZIGBEE_LIGHT;
      zigbee_light_init();
      break;
  }
}

void handle_zigbee_switch_selection() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  switch (selected_item) {
    case ZIGBEE_SWITCH_TOGGLE:
      current_layer = LAYER_ZIGBEE_SWITCH;
      break;
  }
}

void handle_thread_apps_selection() {
  switch (selected_item) {
    case THREAD_MENU_CLI:
      current_layer = LAYER_THREAD_CLI;
      // display_thread_cli();
      break;
  }
}

void handle_gps_selection() {
  switch (selected_item) {
    case GPS_MENU_DATE_TIME:
      current_layer = LAYER_GPS_DATE_TIME;
      break;
    case GPS_MENU_LOCATION:
      current_layer = LAYER_GPS_LOCATION;
      break;
  }
}
