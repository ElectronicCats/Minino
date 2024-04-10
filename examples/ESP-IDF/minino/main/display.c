#include "display.h"
#include "display_helper.h"
#include "esp_log.h"
#include "gps.h"
#include "keyboard_helper.h"
#include "leds.h"
#include "string.h"
#include "thread_cli.h"

#define TIME_ZONE (+8)    // Beijing Time
#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "display";
SH1106_t dev;
uint8_t selected_option;
Layer previous_layer;
Layer current_layer;
int num_items;
uint8_t bluetooth_devices_count;
nmea_parser_handle_t nmea_hdl;
TaskHandle_t wifi_sniffer_task_handle = NULL;

static void gps_event_handler(void* event_handler_arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data);

void display_init() {
  selected_option = 0;
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
  buzzer_play();
  sh1106_bitmaps(&dev, 0, 0, epd_bitmap_logo_1, 128, 64, NO_INVERT);
  buzzer_stop();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
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
void display_clear_line(int page, int invert) {
  sh1106_clear_line(&dev, page, invert);
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
      sprintf(text, " %s", items[i + selected_option]);
    } else if (i == 1) {
      sprintf(text, "  %s", items[i + selected_option]);
    } else {
      sprintf(text, " %s", items[i + selected_option]);
    }

    display_text(text, 0, page, NO_INVERT);
    page += 2;
  }

  display_selected_item_box();
}

void display_scrolling_text(char** text) {
  uint8_t startIdx = (selected_option >= 7) ? selected_option - 6 : 0;
  selected_option =
      (num_items - 2 > 7 && selected_option < 6) ? 6 : selected_option;
  display_clear();
  // ESP_LOGI(TAG, "num: %d", num_items - 2);

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    // ESP_LOGI(TAG, "Text[%d]: %s", i, text[i]);
    if (i == selected_option) {
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

// void display_wifi_sniffer(wifi_sniffer_record_t record) {
//   char* channel_str = (char*) malloc(16);
//   char* ssid_str = (char*) malloc(50);
//   char* rssi_str = (char*) malloc(16);
//   char* addr_str = (char*) malloc(30);
//   char* hash_str = (char*) malloc(16);
//   char* htci_str = (char*) malloc(16);
//   char* sn_str = (char*) malloc(16);
//   char* time_str = (char*) malloc(16);

//   sprintf(channel_str, "Channel=%d", record.channel);
//   display_text("WiFi Sniffer    ", 0, 0, INVERT);
//   display_clear_line(1, NO_INVERT);
//   display_text(channel_str, 0, 1, NO_INVERT);

//   if (record.ssid == NULL && record.timestamp == 0) {
//     return;
//   }

//   sprintf(ssid_str, "SSID=%s", record.ssid);
//   sprintf(addr_str, "ADDR=%02x:%02x:%02x:%02x:%02x:%02x", record.addr[0],
//           record.addr[1], record.addr[2], record.addr[3], record.addr[4],
//           record.addr[5]);
//   sprintf(hash_str, "Hash=%s", record.hash);
//   sprintf(rssi_str, "RSSI=%d", record.rssi);
//   sprintf(htci_str, "HTCI=%s", record.htci);
//   sprintf(sn_str, "SN=%d", record.sn);
//   sprintf(time_str, "Time=%d", (int) record.timestamp);

//   display_clear();
//   display_text("WiFi Sniffer    ", 0, 0, INVERT);
//   display_text(ssid_str, 0, 2, NO_INVERT);
//   display_text(addr_str, 0, 3, NO_INVERT);
//   display_text(hash_str, 0, 4, NO_INVERT);
//   display_text(rssi_str, 0, 5, NO_INVERT);
//   display_text(htci_str, 0, 6, NO_INVERT);
//   // display_text(sn_str, 0, 6, NO_INVERT);
//   display_text(time_str, 0, 7, NO_INVERT);

//   ESP_LOGI(TAG,
//            "ADDR=%02x:%02x:%02x:%02x:%02x:%02x, "
//            "SSID=%s, "
//            "TIMESTAMP=%d, "
//            "HASH=%s, "
//            "RSSI=%02d, "
//            "SN=%d, "
//            "HT CAP. INFO=%s",
//            record.addr[0], record.addr[1], record.addr[2], record.addr[3],
//            record.addr[4], record.addr[5], record.ssid, (int)
//            record.timestamp, record.hash, record.rssi, record.sn,
//            record.htci);
// }

void display_wifi_sniffer_animation_task(void* pvParameter) {
  // display_text("Packets", 64, 0, INVERT);
  // display_text("23", 64, 1, INVERT);
  // display_text("Progress", 64, 3, INVERT);
  // display_text("48%", 64, 4, INVERT);
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
  // display_clear();
  // display_text("Packets", 64, 0, INVERT);
  // display_text("0", 64, 1, INVERT);
  // display_text("Progress", 64, 3, INVERT);
  // display_text("0%", 64, 4, INVERT);
  vTaskResume(wifi_sniffer_task_handle);
}

void display_wifi_sniffer_animation_stop() {
  vTaskSuspend(wifi_sniffer_task_handle);
  // display_text("Timeout!", 64, 6, INVERT);
}

void display_wifi_sniffer_cb(sniffer_runtime_t* sniffer) {
  if (sniffer->is_running) {
    ESP_LOGI(TAG, "sniffer task running");
    display_clear();
    display_text("Packets", 64, 0, INVERT);
    display_text("23", 64, 1, INVERT);
    display_text("Progress", 64, 3, INVERT);
    display_text("48%", 64, 4, INVERT);
  } else {
    ESP_LOGI(TAG, "sniffer task stopped");
  }
}

void display_bluetooth_scanner(bluetooth_scanner_record_t record) {
  static bool airtag_detected = false;
  display_text("Airtags Scanner", 0, 0, INVERT);
  display_clear_line(2, NO_INVERT);

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
  thread_cli_start();

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
