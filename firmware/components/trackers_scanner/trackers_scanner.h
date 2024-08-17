#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#ifndef TRACKERS_SCANNER_H
  #define TRACKERS_SCANNER_H
  #define TAG_BLE_CLIENT_MODULE           "trackers_module:main"
  #define TRACKER_REMOTE_SERVICE_UUID     0x00FF
  #define TRACKER_REMOTE_NOTIFY_CHAR_UUID 0xFF01
  #define TRACKER_PROFILE_NUM             1
  #define TRACKER_PROFILE_A_APP_ID        0
  #define TRACKER_INVALID_HANDLE          0
  #define TRACKER_SCAN_DURATION           60

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
} tracker_profile_t;

/**
 * @brief Structure to store the tracker advertisement comparison
 *
 */
typedef struct {
  uint8_t adv_cmp[4];
  char* name;
  char* vendor;
} tracker_adv_cmp_t;

/**
 * @brief Callback to handle the bluetooth scanner
 *
 * @param record The tracker profile record
 */
typedef void (*bluetooth_traker_scanner_cb_t)(tracker_profile_t record);

/**
 * @brief Register the callback to handle the bluetooth scanner
 *
 * @param cb The callback to handle the bluetooth scanner
 */
void trackers_scanner_register_cb(bluetooth_traker_scanner_cb_t cb);

/**
 * @brief Start the bluetooth scanner
 *
 */
void trackers_scanner_start();

/**
 * @brief Stop the bluetooth scanner
 *
 */
void trackers_scanner_stop();

/**
 * @brief Check if the bluetooth scanner is active
 *
 * @return true The bluetooth scanner is active
 * @return false The bluetooth scanner is not active
 */
bool trackers_scanner_is_active();

/**
 * @brief Add a tracker profile to the list
 *
 * @param profiles The list of tracker profiles
 * @param num_profiles The number of tracker profiles
 * @param new_profile The new tracker profile to add
 */
void trackers_scanner_add_tracker_profile(tracker_profile_t** profiles,
                                          uint16_t* num_profiles,
                                          tracker_profile_t new_profile);

/**
 * @brief Find a profile by the mac address
 *
 * @param profiles Set of profiles
 * @param num_profiles Number of profiles
 * @param mac_address Mac address to find
 * @return int Index of the profile or -1 if not found
 */
int trackers_scanner_find_profile_by_mac(tracker_profile_t* profiles,
                                         uint16_t num_profiles,
                                         uint8_t mac_address[6]);
#endif  // TRACKERS_SCANNER_H
