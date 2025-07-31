#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gps_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "wardriving_module.h"
#include "wardriving_screens_module.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

#include "wardriving_common.h"

#define FILE_NAME                   WARFI_DIR_NAME "/Warfi"
#define CSV_FILE_SIZE               8192
#define CSV_HEADER_LINES            1
#define MAX_CSV_LINES               20
#define WIFI_SCAN_REFRESH_RATE_MS   3000
#define DISPLAY_REFRESH_RATE_SEC    2
#define WRITE_FILE_REFRESH_RATE_SEC 5
#define MAX_MAC_TABLE_SIZE          100
#define MAC_TIMEOUT_SEC             30

typedef enum {
  WARDRIVING_MODULE_STATE_NO_SD_CARD = 0,
  WARDRIVING_MODULE_STATE_INVALID_SD_CARD,
  WARDRIVING_MODULE_STATE_SCANNING,
  WARDRIVING_MODULE_STATE_STOPPED
} wardriving_module_state_t;

typedef struct {
  uint8_t mac[6];      // MAC (6 bytes)
  uint32_t timestamp;  // last time seen
} mac_entry_t;

static const char* TAG = "wardriving_module";
wardriving_module_state_t wardriving_module_state =
    WARDRIVING_MODULE_STATE_STOPPED;
TaskHandle_t wardriving_module_scan_task_handle = NULL;
TaskHandle_t scanning_wifi_animation_task_handle = NULL;
bool running_wifi_scanner_animation = false;

uint16_t csv_lines = 0;
uint16_t wifi_scanned_packets = 0;
char* csv_file_name = NULL;
char* csv_file_buffer = NULL;
bool csv_file_initialized = false;

// MACs table
mac_entry_t mac_table[MAX_MAC_TABLE_SIZE];
uint16_t mac_table_count = 0;
uint16_t mac_table_head = 0;

const char* csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
    "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";

char* get_mac_address(uint8_t* mac) {
  char* mac_address = malloc(18);
  if (mac_address == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for mac_address");
    return NULL;
  }
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

uint16_t get_frequency(uint8_t primary) {
  return 2412 + 5 * (primary - 1);
}

// MAC in list?
bool is_mac_in_table(uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < mac_table_count; i++) {
    if (memcmp(mac_table[i].mac, mac, 6) == 0) {
      if (current_time - mac_table[i].timestamp < MAC_TIMEOUT_SEC) {
        return true;  // MAC found
      } else {
        // Update timestamp
        mac_table[i].timestamp = current_time;
        return false;  // new MAC
      }
    }
  }
  return false;
}

// Update MAC in table
void add_mac_to_table(uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < mac_table_count; i++) {
    if (memcmp(mac_table[i].mac, mac, 6) == 0) {
      mac_table[i].timestamp = current_time;  // Update timestamp
      return;
    }
  }
  if (mac_table_count < MAX_MAC_TABLE_SIZE) {
    memcpy(mac_table[mac_table_count].mac, mac, 6);
    mac_table[mac_table_count].timestamp = current_time;
    mac_table_count++;
  } else {
    memcpy(mac_table[mac_table_head].mac, mac, 6);
    mac_table[mac_table_head].timestamp = current_time;
    mac_table_head = (mac_table_head + 1) % MAX_MAC_TABLE_SIZE;
  }
}

void wardriving_module_scan_task(void* pvParameters) {
  while (true) {
    wifi_scanner_module_scan();
    vTaskDelay(WIFI_SCAN_REFRESH_RATE_MS / portTICK_PERIOD_MS);
  }
}

static void update_file_name(gps_t* gps) {
  if (csv_file_name == NULL) {
    csv_file_name = malloc(strlen(FILE_NAME) + 30);
    if (csv_file_name == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_name");
      return;
    }
  }

  char* full_date_time = get_full_date_time(gps);
  if (full_date_time == NULL) {
    ESP_LOGE(TAG, "Failed to get full date time");
    return;
  }

  snprintf(csv_file_name, strlen(FILE_NAME) + 30, "%s_%s.csv", FILE_NAME,
           full_date_time);
  for (int i = 0; i < strlen(csv_file_name); i++) {
    if (csv_file_name[i] == ' ')
      csv_file_name[i] = '_';
    if (csv_file_name[i] == ':')
      csv_file_name[i] = '-';
  }
  free(full_date_time);
}

static void wardriving_module_save_to_file(gps_t* gps) {
  if (gps->sats_in_use == 0) {
    static uint32_t no_signal_counter = 0;
    no_signal_counter++;
    if (no_signal_counter >= 10) {  // No signal message
      wardriving_screens_module_no_gps_signal();
      no_signal_counter = 0;
    }
    ESP_LOGW(TAG, "No GPS signal, skipping save");
    return;
  }

  esp_err_t err = sd_card_create_dir(WARFI_DIR_NAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create %s directory: %s", WARFI_DIR_NAME,
             esp_err_to_name(err));
    wardriving_module_state = WARDRIVING_MODULE_STATE_NO_SD_CARD;
    wardriving_screens_module_no_sd_card();
    return;
  }

  if (!csv_file_initialized) {
    update_file_name(gps);
    if (csv_file_name == NULL) {
      ESP_LOGE(TAG, "Failed to initialize file name");
      return;
    }
    snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", csv_header);
    err = sd_card_append_to_file(csv_file_name, csv_file_buffer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write CSV header: %s", esp_err_to_name(err));
      return;
    }
    csv_file_buffer[0] = '\0';
    csv_file_initialized = true;
    csv_lines = CSV_HEADER_LINES;
  }

  wifi_scanner_ap_records_t* ap_records = wifi_scanner_get_ap_records();
  char csv_line_buffer[CSV_LINE_SIZE];
  uint32_t current_time = esp_timer_get_time() / 1000000;

  for (int i = 0; i < ap_records->count; i++) {
    if (is_mac_in_table(ap_records->records[i].bssid, current_time)) {
      ESP_LOGD(TAG, "Skipping duplicate MAC");
      continue;
    }

    char* auth_mode_str = get_auth_mode(ap_records->records[i].authmode);
    char* mac_address_str = get_mac_address(ap_records->records[i].bssid);
    if (mac_address_str == NULL) {
      continue;
    }

    char sanitized_ssid[33];
    strncpy(sanitized_ssid, (const char*) ap_records->records[i].ssid, 32);
    sanitized_ssid[32] = '\0';
    char *src = sanitized_ssid, *dst = sanitized_ssid;
    while (*src) {
      if (*src != ',') {
        *dst++ = *src;
      }
      src++;
    }
    *dst = '\0';

    char* full_date_time = get_full_date_time(gps);
    if (full_date_time == NULL) {
      free(mac_address_str);
      continue;
    }

    if (strcmp(mac_address_str, EMPTY_MAC_ADDRESS) == 0) {
      free(mac_address_str);
      free(full_date_time);
      continue;
    }

    snprintf(csv_line_buffer, CSV_LINE_SIZE,
             "%s,%s,%s,%s,%d,%u,%d,%f,%f,%f,%f,%s,%s,%s\n", mac_address_str,
             sanitized_ssid, auth_mode_str, full_date_time,
             ap_records->records[i].primary,
             get_frequency(ap_records->records[i].primary),
             ap_records->records[i].rssi, gps->latitude, gps->longitude,
             gps->altitude, GPS_ACCURACY, "", "", "WIFI");

    add_mac_to_table(ap_records->records[i].bssid, current_time);

    free(mac_address_str);
    free(full_date_time);

    if (strlen(csv_file_buffer) + strlen(csv_line_buffer) < CSV_FILE_SIZE) {
      strcat(csv_file_buffer, csv_line_buffer);
      csv_lines++;
      wifi_scanned_packets++;
    } else {
      ESP_LOGW(TAG, "Buffer full, appending to SD");
      err = sd_card_append_to_file(csv_file_name, csv_file_buffer);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to append to SD: %s", esp_err_to_name(err));
      }
      csv_file_buffer[0] = '\0';
      csv_lines = CSV_HEADER_LINES;
      strcat(csv_file_buffer, csv_line_buffer);
      csv_lines++;
      wifi_scanned_packets++;
    }
  }
}

void wardriving_gps_event_handler_cb(gps_t* gps) {
  static uint32_t counter = 0;
  counter++;

  if (gps->sats_in_use == 0) {
    vTaskSuspend(scanning_wifi_animation_task_handle);
    running_wifi_scanner_animation = false;
    wardriving_screens_module_no_gps_signal();
    return;
  }

  if (!running_wifi_scanner_animation) {
    vTaskResume(scanning_wifi_animation_task_handle);
    running_wifi_scanner_animation = true;
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
  csv_file_initialized = false;
  mac_table_count = 0;
  mac_table_head = 0;

  ESP_LOGI(TAG, "Free heap size before allocation: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  ESP_LOGI(TAG, "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
  csv_file_buffer = malloc(CSV_FILE_SIZE);
  if (csv_file_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
    return;
  }
  csv_file_buffer[0] = '\0';
}

void wardriving_module_end() {
  ESP_LOGI(TAG, "Wardriving module end");
  esp_err_t err = sd_card_unmount();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "SD card unmounted successfully");
  }

  if (csv_file_buffer != NULL) {
    free(csv_file_buffer);
    csv_file_buffer = NULL;
  }
  if (csv_file_name != NULL) {
    free(csv_file_name);
    csv_file_name = NULL;
  }
  csv_file_initialized = false;
  csv_lines = 0;
  wifi_scanned_packets = 0;
  mac_table_count = 0;
  mac_table_head = 0;
}

void wardriving_module_start_scan() {
  menus_module_set_app_state(true, wardriving_module_keyboard_cb);
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }
  ESP_LOGI(TAG, "Start scan");
  wardriving_module_state = WARDRIVING_MODULE_STATE_SCANNING;
  xTaskCreate(wardriving_module_scan_task, "wardriving_module_scan_task", 4096,
              NULL, 5, &wardriving_module_scan_task_handle);
  xTaskCreate(wardriving_screens_wifi_animation_task,
              "scanning_wifi_animation_task", 4096, NULL, 5,
              &scanning_wifi_animation_task_handle);
  vTaskSuspend(scanning_wifi_animation_task_handle);
  running_wifi_scanner_animation = false;

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

  if (wardriving_module_scan_task_handle != NULL) {
    vTaskDelete(wardriving_module_scan_task_handle);
    wardriving_module_scan_task_handle = NULL;
    ESP_LOGI(TAG, "Task wardriving_module_scan_task deleted");
  }

  if (scanning_wifi_animation_task_handle != NULL) {
    vTaskDelete(scanning_wifi_animation_task_handle);
    scanning_wifi_animation_task_handle = NULL;
    ESP_LOGI(TAG, "Task scanning_wifi_animation_task deleted");
  }

  if (csv_file_buffer != NULL && csv_file_initialized &&
      csv_file_buffer[0] != '\0') {
    ESP_LOGI(TAG, "Appending final data to %s", csv_file_name);
    esp_err_t err = sd_card_append_to_file(csv_file_name, csv_file_buffer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to append final data: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "Final data appended successfully");
    }
  }

  csv_file_initialized = false;
  csv_lines = 0;
  wifi_scanned_packets = 0;
  mac_table_count = 0;
  mac_table_head = 0;
}

void wardriving_module_keyboard_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      wardriving_module_stop_scan();
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      if (wardriving_module_state == WARDRIVING_MODULE_STATE_NO_SD_CARD) {
        wardriving_module_start_scan();
      } else if (wardriving_module_state ==
                 WARDRIVING_MODULE_STATE_INVALID_SD_CARD) {
        wardriving_screens_module_formating_sd_card();
        sd_card_settings_format();
        esp_err_t err = sd_card_check_format();
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