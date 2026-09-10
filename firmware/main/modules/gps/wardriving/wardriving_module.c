#include <string.h>
#include "esp_event.h"  // Añadido para manejar el bucle de eventos
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"  // Añadido para funciones de desinicialización de WiFi
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "error_handler.h"
#include "gps_module.h"
#include "memory_monitor.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "task_manager.h"
#include "wardriving_common.h"
#include "wardriving_module.h"
#include "wardriving_screens_module.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

#define FILE_NAME                   WARFI_DIR_NAME "/Warfi"
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
  uint8_t mac[6];
  uint32_t timestamp;
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

static void format_mac_address(const uint8_t* mac, char* buf, size_t len) {
  if (mac == NULL || buf == NULL || len < 18) {
    return;
  }
  snprintf(buf, len, MAC_ADDRESS_FORMAT, mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
}

static void format_full_date_time(const gps_t* gps, char* buf, size_t len) {
  if (buf == NULL || len < 32) {
    return;
  }
  if (gps != NULL && (gps->date.year > 0 || gps->sats_in_use > 0)) {
    uint16_t year = gps->date.year;
    if (year < 100) {
      year += 2000;
    }
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u", year, gps->date.month,
             gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  } else {
    snprintf(buf, len, "2000-01-01 00:00:00");
  }
}

const char* get_auth_mode(int authmode) {
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

bool is_mac_in_table(const uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < mac_table_count; i++) {
    if (memcmp(mac_table[i].mac, mac, 6) == 0) {
      if (current_time - mac_table[i].timestamp < MAC_TIMEOUT_SEC) {
        return true;
      } else {
        mac_table[i].timestamp = current_time;
        return false;
      }
    }
  }
  return false;
}

void add_mac_to_table(const uint8_t* mac, uint32_t current_time) {
  for (uint16_t i = 0; i < mac_table_count; i++) {
    if (memcmp(mac_table[i].mac, mac, 6) == 0) {
      mac_table[i].timestamp = current_time;
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
    csv_file_name = malloc(strlen(FILE_NAME) + 48);
    if (csv_file_name == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_name");
      return;
    }
  }

  char full_date_time[32];
  format_full_date_time(gps, full_date_time, sizeof(full_date_time));

  snprintf(csv_file_name, strlen(FILE_NAME) + 48, "%s_%s.csv", FILE_NAME,
           full_date_time);
  for (size_t i = 0; i < strlen(csv_file_name); i++) {
    if (csv_file_name[i] == ' ')
      csv_file_name[i] = '_';
    if (csv_file_name[i] == ':')
      csv_file_name[i] = '-';
  }
}

static void wardriving_module_save_to_file(gps_t* gps) {
  if (gps == NULL || gps->sats_in_use == 0) {
    static uint32_t no_signal_counter = 0;
    no_signal_counter++;
    if (no_signal_counter >= 10) {
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

  if (csv_file_buffer == NULL) {
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }
    csv_file_buffer[0] = '\0';
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
  if (ap_records == NULL) {
    return;
  }

  char csv_line_buffer[CSV_LINE_SIZE];
  char mac_address_str[18];
  char full_date_time[32];
  uint32_t current_time = (uint32_t) (esp_timer_get_time() / 1000000);

  format_full_date_time(gps, full_date_time, sizeof(full_date_time));

  for (int i = 0; i < ap_records->count; i++) {
    if (is_mac_in_table(ap_records->records[i].bssid, current_time)) {
      ESP_LOGD(TAG, "Skipping duplicate MAC");
      continue;
    }

    format_mac_address(ap_records->records[i].bssid, mac_address_str,
                       sizeof(mac_address_str));

    if (strcmp(mac_address_str, EMPTY_MAC_ADDRESS) == 0) {
      continue;
    }

    const char* auth_mode_str = get_auth_mode(ap_records->records[i].authmode);

    snprintf(csv_line_buffer, sizeof(csv_line_buffer),
             "%s,%s,%s,%s,%d,%u,%d,%f,%f,%f,%f,%s,%s,%s\n", mac_address_str,
             ap_records->records[i].ssid, auth_mode_str, full_date_time,
             ap_records->records[i].primary,
             get_frequency(ap_records->records[i].primary),
             ap_records->records[i].rssi, gps->latitude, gps->longitude,
             gps->altitude, (double) GPS_ACCURACY, "", "", "WIFI");

    add_mac_to_table(ap_records->records[i].bssid, current_time);

    size_t cur_len = strlen(csv_file_buffer);
    size_t line_len = strlen(csv_line_buffer);

    if (cur_len + line_len >= CSV_FILE_SIZE - 1) {
      ESP_LOGI(TAG, "Flushing wardriving buffer to SD");
      err = sd_card_append_to_file(csv_file_name, csv_file_buffer);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to append to SD: %s", esp_err_to_name(err));
      }
      csv_file_buffer[0] = '\0';
      csv_lines = CSV_HEADER_LINES;
    }

    strncat(csv_file_buffer, csv_line_buffer,
            CSV_FILE_SIZE - strlen(csv_file_buffer) - 1);
    csv_lines++;
    wifi_scanned_packets++;
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

  if (csv_file_buffer == NULL) {
    ESP_LOGI(TAG, "Free heap size before allocation: %" PRIu32 " bytes",
             esp_get_free_heap_size());
    ESP_LOGI(TAG, "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }
  }
  csv_file_buffer[0] = '\0';
}

void wardriving_module_end() {
  ESP_LOGI(TAG, "Wardriving module end");

  // Desmontar la tarjeta SD
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
    ESP_LOGE(TAG, "SD card verification failed");
    return;
  }

  if (csv_file_buffer == NULL) {
    wardriving_module_begin();
  }

  ESP_LOGI(TAG, "Start scan");
  wardriving_module_state = WARDRIVING_MODULE_STATE_SCANNING;

  // Usar Task Manager para tareas críticas
  esp_err_t err = task_manager_create(
      wardriving_module_scan_task, "wardriving_scan", TASK_STACK_MEDIUM, NULL,
      TASK_PRIORITY_HIGH,  // GPS es alta prioridad
      &wardriving_module_scan_task_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create wardriving scan task");
    return;
  }

  task_manager_create(
      wardriving_screens_wifi_animation_task, "wardriving_anim",
      TASK_STACK_SMALL,  // Animaciones solo necesitan stack pequeño
      NULL,
      TASK_PRIORITY_LOW,  // UI baja prioridad
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

  // Desinicializar el controlador WiFi
  wifi_driver_deinit();

  // Usar Task Manager para eliminar tareas
  if (wardriving_module_scan_task_handle != NULL) {
    task_manager_delete(wardriving_module_scan_task_handle);
    wardriving_module_scan_task_handle = NULL;
  }

  if (scanning_wifi_animation_task_handle != NULL) {
    task_manager_delete(scanning_wifi_animation_task_handle);
    scanning_wifi_animation_task_handle = NULL;
  }

  if (csv_file_buffer != NULL && csv_file_initialized &&
      csv_file_buffer[0] != '\0' && csv_file_name != NULL) {
    ESP_LOGI(TAG, "Appending final data to %s", csv_file_name);
    esp_err_t err = sd_card_append_to_file(csv_file_name, csv_file_buffer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to append final data: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "Final data appended successfully");
    }
  }

  wardriving_module_end();
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