#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#ifndef BLE_SCANNER_H
  #define BLE_SCANNER_H
  #define TAG_BLE_CLIENT_MODULE           "scanner_module:main"
  #define SCANNER_REMOTE_SERVICE_UUID     0x00FF
  #define SCANNER_REMOTE_NOTIFY_CHAR_UUID 0xFF01
  #define SCANNER_PROFILE_NUM             1
  #define SCANNER_PROFILE_A_APP_ID        0
  #define SCANNER_INVALID_HANDLE          0
  #define SCANNER_SCAN_DURATION           60

/**
 * @brief Structure to store the tracker profile
 *
 */
typedef struct {
  int rssi;
  char* name;
  char* vendor;
  uint8_t mac_address[6];
  uint8_t adv_data[31];
  uint8_t adv_data_length;
  bool is_tracker;
} device_profile;

/**
 * @brief Structure to store the tracker advertisement comparison
 *
 */
typedef struct {
  uint8_t adv_cmp[4];
  char* name;
  char* vendor;
} scanner_adv_cmp_t;

/**
 * @brief Callback to handle the bluetooth scanner
 *
 * @param record The tracker profile record
 */
typedef void (*bluetooth_adv_scanner_cb_t)(esp_ble_gap_cb_param_t* record);

/**
 * @brief Register the callback to handle the bluetooth scanner
 *
 * @param cb The callback to handle the bluetooth scanner
 */
void ble_scanner_register_cb(bluetooth_adv_scanner_cb_t cb);

/**
 * @brief Start the bluetooth scanner
 *
 */
void ble_scanner_begin();

/**
 * @brief Stop the bluetooth scanner
 *
 */
void ble_scanner_stop();

/**
 * @brief Check if the bluetooth scanner is active
 *
 * @return true The bluetooth scanner is active
 * @return false The bluetooth scanner is not active
 */
bool ble_scanner_is_active();
void set_filter_type(uint8_t filter_type);
void set_scan_type(uint8_t scan_type);
#endif  // BLE_SCANNER_H
