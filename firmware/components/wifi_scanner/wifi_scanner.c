#include "wifi_scanner.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"

/**
 * @brief Stores last scanned AP records into linked list.
 *
 */
static wifi_scanner_ap_records_t ap_records;

void wifi_scanner_module_scan() {
#if !defined(CONFIG_WIFI_SCANNER_DEBUG)
  esp_log_level_set(TAG_WIFI_SCANNER_MODULE, ESP_LOG_NONE);
#endif

  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_SCANNER_MODULE, "Failed to create event loop: %s",
             esp_err_to_name(err));
    esp_event_loop_delete_default();
    esp_event_loop_create_default();
  }

  ap_records.count = CONFIG_SCAN_MAX_AP;
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_clear_ap_list());
  ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
  ESP_LOGI(TAG_WIFI_SCANNER_MODULE, "Max AP number ap_info can hold = %u",
           ap_records.count);
  ESP_ERROR_CHECK(
      esp_wifi_scan_get_ap_records(&ap_records.count, ap_records.records));
  ESP_LOGI(TAG_WIFI_SCANNER_MODULE, "Found %u APs.", ap_records.count);
  ESP_LOGD(TAG_WIFI_SCANNER_MODULE, "Scan done.");
}

wifi_scanner_ap_records_t* wifi_scanner_get_ap_records() {
  return &ap_records;
}

wifi_ap_record_t* wifi_scanner_get_ap_record(unsigned index) {
  if (index > ap_records.count) {
    ESP_LOGE(TAG_WIFI_SCANNER_MODULE,
             "Index out of bounds! %u records available, but %u requested",
             ap_records.count, index);
    return NULL;
  }
  return &ap_records.records[index];
}

void wifi_scanner_clear_ap_records() {
  memset(&ap_records, 0, sizeof(ap_records));
  esp_wifi_clear_ap_list();
}