#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gps_module.h"
#include "menus_module.h"
#include "sd_card.h"
#include "wardriving_module.h"
#include "wardriving_screens_module.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

#define DIR_NAME       "Wardriving"
#define FILE_NAME      DIR_NAME "/Minino"
#define FORMAT_VERSION "WigleWifi-1.6"
#define APP_VERSION    CONFIG_PROJECT_VERSION
#define MODEL          "MININO"
#define RELEASE        APP_VERSION
#define DEVICE         "MININO"
#define DISPLAY        "SH1106 OLED"
#define BOARD          "ESP32C6"
#define BRAND          "Electronic Cats"
#define STAR           "Sol"
#define BODY           "3"
#define SUB_BODY       "0"

#define MAC_ADDRESS_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"
#define EMPTY_MAC_ADDRESS  "00:00:00:00:00:00"
#define MAX_CSV_LINES      100
#define CSV_LINE_SIZE      200  // Got it from real time tests
#define CSV_FILE_SIZE      CSV_LINE_SIZE* MAX_CSV_LINES
#define CSV_HEADER_LINES   2  // Check `csv_header` variable

#define WIFI_SCAN_REFRESH_RATE_MS   3000
#define DISPLAY_REFRESH_RATE_SEC    2
#define WRITE_FILE_REFRESH_RATE_SEC 5

typedef enum {
  WARDRIVING_MODULE_STATE_NO_SD_CARD = 0,
  WARDRIVING_MODULE_STATE_INVALID_SD_CARD,
  WARDRIVING_MODULE_STATE_SCANNING,
  WARDRIVING_MODULE_STATE_STOPPED
} wardriving_module_state_t;

const char* TAG = "wardriving";
wardriving_module_state_t wardriving_module_state =
    WARDRIVING_MODULE_STATE_STOPPED;
TaskHandle_t wardriving_module_scan_task_handle = NULL;
uint16_t csv_lines;
uint16_t wifi_scanned_packets;
char* csv_file_name = NULL;
char* csv_file_buffer = NULL;

const char* csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
    "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";

char* get_mac_address(uint8_t* mac) {
  char* mac_address = malloc(18);
  sprintf(mac_address, MAC_ADDRESS_FORMAT, mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  return mac_address;
}

char* get_auth_mode(int authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI_PSK";
    case WIFI_AUTH_OWE:
      return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:
      return "WPA3_ENT_SUITE_B_192_BIT";
    case WIFI_AUTH_DPP:
      return "DPP";
    default:
      return "Uncategorized";
  }
}

char* get_full_date_time(gps_t* gps) {
  char* date_time = malloc(sizeof(char) * 30);
  sprintf(date_time, "%d-%d-%d %d:%d:%d", gps->date.year, gps->date.month,
          gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  return date_time;
}

uint16_t get_frequency(uint8_t primary) {
  return 2412 + 5 * (primary - 1);
}

void wardriving_module_scan_task(void* pvParameters) {
  wifi_driver_init_sta();

  while (true) {
    wifi_scanner_module_scan();
    vTaskDelay(WIFI_SCAN_REFRESH_RATE_MS / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Save the scanned AP records to a CSV file
 *
 * @param gps The GPS module instance
 *
 * @note This function is called every WRITE_FILE_REFRESH_RATE_SEC seconds to
 * save the scanned AP records to a CSV file in the SD card.
 *
 * @return void
 */
void wardriving_module_save_to_file(gps_t* gps) {
  esp_err_t err = sd_card_create_dir(DIR_NAME);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create directory");
    return;
  }

  wifi_scanner_ap_records_t* ap_records = wifi_scanner_get_ap_records();
  char* csv_line_buffer = malloc(CSV_LINE_SIZE);

  // Append records to csv file buffer
  for (int i = 0; i < ap_records->count; i++) {
    csv_lines++;
    wifi_scanned_packets++;
    char* mac_address_str = get_mac_address(ap_records->records[i].bssid);
    char* auth_mode_str = get_auth_mode(ap_records->records[i].authmode);
    char* full_date_time = get_full_date_time(gps);

    // +1 because there is a csv_lines++ before this
    if (csv_lines == CSV_HEADER_LINES + 1) {
      sprintf(csv_file_name, "%s_%s.csv", FILE_NAME, full_date_time);
      // Replace " " by "_" and ":" by "-"
      for (int i = 0; i < strlen(csv_file_name); i++) {
        if (csv_file_name[i] == ' ') {
          csv_file_name[i] = '_';
        }
        if (csv_file_name[i] == ':') {
          csv_file_name[i] = '-';
        }
      }
    }

    if (csv_lines >= MAX_CSV_LINES) {
      ESP_LOGW(TAG, "Max CSV lines reached, writing to file");
      sd_card_write_file(csv_file_name, csv_file_buffer);
      csv_lines = CSV_HEADER_LINES;
      free(csv_file_buffer);
      csv_file_buffer = malloc(CSV_FILE_SIZE);
      sprintf(csv_file_buffer, "%s\n",
              csv_header);  // Append header to csv file
    }

    if (strcmp(mac_address_str, EMPTY_MAC_ADDRESS) == 0) {
      // ESP_LOGW(TAG, "Empty MAC address found, skipping");
      free(mac_address_str);
      free(full_date_time);
      continue;
    }

    sprintf(csv_line_buffer, "%s,%s,%s,%s,%d,%u,%d,%f,%f,%f,%f,%s,%s,%s\n",
            /* MAC */
            mac_address_str,
            /* SSID */
            ap_records->records[i].ssid,
            /* AuthMode */
            auth_mode_str,
            /* FirstSeen */
            full_date_time,
            /* Channel */
            ap_records->records[i].primary,
            /* Frequency */
            get_frequency(ap_records->records[i].primary),
            /* RSSI */
            ap_records->records[i].rssi,
            /* CurrentLatitude */
            gps->latitude,
            /* CurrentLongitude */
            gps->longitude,
            /* AltitudeMeters */
            gps->altitude,
            /* AccuracyMeters */
            GPS_ACCURACY,
            /* RCOIs */
            "",
            /* MfgrId */
            "",
            /* Type */
            "WIFI");

    free(mac_address_str);
    free(full_date_time);

    // ESP_LOGI(TAG, "CSV Line: %s", csv_line_buffer);
    // ESP_LOGI(TAG, "Line size %d bytes", strlen(csv_line_buffer));
    strcat(csv_file_buffer, csv_line_buffer);
  }
  free(csv_line_buffer);
  ESP_LOGI(TAG, "File size %d bytes, scanned packets: %u",
           strlen(csv_file_buffer), wifi_scanned_packets);
  sd_card_write_file(csv_file_name, csv_file_buffer);
}

/**
 * @brief Callback function for GPS events
 *
 * @param event_data
 *
 * @note This function is called every time a GPS event is triggered, by default
 * is 1Hz (1 second) according to the ATGM336H-6N-74 datasheet.
 */
void wardriving_gps_event_handler_cb(gps_t* gps) {
  static uint32_t counter = 0;
  counter++;

  ESP_LOGI(TAG,
           "Satellites in use: %d, signal: %s \r\n"
           "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
           "\t\t\t\t\t\tlongitude = %.05f°E\r\n",
           gps->sats_in_use, gps_module_get_signal_strength(gps), gps->latitude,
           gps->longitude);

  if (gps->sats_in_use == 0) {
    wardriving_screens_module_no_gps_signal();
    return;
  }

  if (counter % DISPLAY_REFRESH_RATE_SEC == 0 || counter == 1) {
    wardriving_screens_module_scanning(wifi_scanned_packets,
                                       gps_module_get_signal_strength(gps));
  }

  if (counter % WRITE_FILE_REFRESH_RATE_SEC == 0) {
    wardriving_module_save_to_file(gps);
  }
}

esp_err_t wardriving_module_verify_sd_card() {
  ESP_LOGI(TAG, "Verifying SD card");
  esp_err_t err = sd_card_mount();
  if (err == ESP_ERR_NOT_SUPPORTED) {
    wardriving_module_state = WARDRIVING_MODULE_STATE_INVALID_SD_CARD;
    wardriving_screens_module_format_sd_card();
  } else if (err != ESP_OK) {
    wardriving_module_state = WARDRIVING_MODULE_STATE_NO_SD_CARD;
    wardriving_screens_module_no_sd_card();
  }
  return err;
}

void wardriving_module_begin() {
#if !defined(CONFIG_WARDRIVING_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  ESP_LOGI(TAG, "Wardriving module begin");
  csv_lines = CSV_HEADER_LINES;
  wifi_scanned_packets = 0;
  csv_file_buffer = malloc(CSV_FILE_SIZE);
  csv_file_name = malloc(strlen(FILE_NAME) + 30);
  sprintf(csv_file_name, "%s.csv", FILE_NAME);
  sprintf(csv_file_buffer, "%s\n", csv_header);  // Append header to csv file
}

void wardriving_module_end() {
  ESP_LOGI(TAG, "Wardriving module end");
  free(csv_file_buffer);
  free(csv_file_name);
}

void wardriving_module_start_scan() {
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }
  menus_module_set_app_state(true, wardriving_module_keyboard_cb);
  ESP_LOGI(TAG, "Start scan");
  wardriving_module_state = WARDRIVING_MODULE_STATE_SCANNING;
  xTaskCreate(wardriving_module_scan_task, "wardriving_module_scan_task", 4096,
              NULL, 5, &wardriving_module_scan_task_handle);
  gps_module_register_cb(wardriving_gps_event_handler_cb);
  wardriving_screens_module_loading_text();
  gps_module_start_scan();
}

void wardriving_module_stop_scan() {
  if (wardriving_module_state != WARDRIVING_MODULE_STATE_SCANNING) {
    return;
  }

  ESP_LOGI(TAG, "Stop scan");
  wardriving_module_state = WARDRIVING_MODULE_STATE_STOPPED;
  gps_module_stop_read();
  gps_module_unregister_cb();
  wifi_driver_deinit();
  sd_card_read_file(csv_file_name);
  sd_card_unmount();
  if (wardriving_module_scan_task_handle != NULL) {
    vTaskDelete(wardriving_module_scan_task_handle);
    wardriving_module_scan_task_handle = NULL;
    ESP_LOGI(TAG, "Task deleted");
  }
}

void wardriving_module_keyboard_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_SINGLE_CLICK) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      if (wardriving_module_state == WARDRIVING_MODULE_STATE_NO_SD_CARD) {
        wardriving_module_start_scan();
      } else if (wardriving_module_state ==
                 WARDRIVING_MODULE_STATE_INVALID_SD_CARD) {
        wardriving_screens_module_formating_sd_card();
        esp_err_t err = sd_card_format();
        if (err == ESP_OK) {
          ESP_LOGI(TAG, "Format done");
          wardriving_module_start_scan();
        } else {
          wardriving_screens_module_failed_format_sd_card();
        }
      }
      break;
    default:
      break;
  }
}
