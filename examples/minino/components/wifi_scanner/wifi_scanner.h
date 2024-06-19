#include "esp_wifi.h"
#ifndef WIFI_SCANNER_MODULE_H
  #define WIFI_SCANNER_MODULE_H
  #define TAG_WIFI_SCANNER_MODULE "wifi_scanner"
  #define DEFAULT_SCAN_LIST_SIZE  CONFIG_SCAN_MAX_AP

/**
 * @brief Linked list of wifi_ap_record_t records.
 *
 */
typedef struct {
  uint16_t count;
  wifi_ap_record_t records[CONFIG_SCAN_MAX_AP];
} wifi_scanner_ap_records_t;

/**
 * @brief Returns current list of scanned APs.
 *
 * @return wifi_scanner_ap_records_t*
 */
wifi_scanner_ap_records_t* wifi_scanner_get_ap_records();

/**
 * @brief Returns AP record on given index
 *
 * @param index
 * @return wifi_ap_record_t*
 */
wifi_ap_record_t* wifi_scanner_get_ap_record(unsigned index);

/**
 * @brief Start the wifi scanner module
 *
 */
void wifi_scanner_module_scan();
#endif  // WIFI_SCANNER_MODULE_H
