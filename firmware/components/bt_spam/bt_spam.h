
#ifndef BLE_SPAM_H
#define BLE_SPAM_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_bt.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"

#define TAG_BLE_SPAM "ble_spam:main"

typedef enum {
  BT_SPAM_MODE_ALL = 0,
  BT_SPAM_MODE_APPLE,
  BT_SPAM_MODE_ANDROID,
  BT_SPAM_MODE_WINDOWS,
  BT_SPAM_MODE_SAMSUNG,
  BT_SPAM_MODE_MAX,
} bt_spam_mode_t;

typedef void (*bt_spam_cb_display)(const char* name);

/**
 * @brief Initialize the bluetooth spam module with specified mode
 *
 * @param mode Target platform mode
 */
void bt_spam_app_main(bt_spam_mode_t mode);

/**
 * @brief Register the callback to display the spam data
 *
 * @param callback The callback to display the spam data
 */
void bt_spam_register_cb(bt_spam_cb_display callback);

/**
 * @brief Stop the bluetooth spam module and release resources
 */
void bt_spam_app_stop(void);

/**
 * @brief Check if the BLE spam module is currently running
 */
bool bt_spam_is_running(void);

/**
 * @brief Fully deinitialize the bluetooth spam module (BT controller +
 * Bluedroid) Call this when completely shutting down BLE functionality
 */
void bt_spam_app_deinit(void);

/**
 * @brief Set BLE advertising TX power level
 * @param power_level ESP_PWR_LVL_P9, ESP_PWR_LVL_P12, ESP_PWR_LVL_P15,
 * ESP_PWR_LVL_P18, ESP_PWR_LVL_P20
 * @return ESP_OK on success
 */
esp_err_t bt_spam_set_tx_power(esp_power_level_t power_level);

/**
 * @brief Set advertising interval (for power/performance tradeoff)
 * @param min_interval_min Minimum advertising interval in 0.625ms units (0x20 =
 * 20ms)
 * @param max_interval_max Maximum advertising interval in 0.625ms units (0x30 =
 * 30ms)
 * @return ESP_OK on success
 */
esp_err_t bt_spam_set_adv_interval(uint16_t min_interval_min,
                                   uint16_t max_interval_max);

/**
 * @brief Get heap statistics during spam operation
 * @param free_kb Output: current free heap in KB
 * @param min_kb Output: minimum free heap during spam in KB
 * @param fragmentation Output: fragmentation percentage
 * @return ESP_OK on success
 */
esp_err_t bt_spam_get_heap_stats(uint32_t* free_kb,
                                 uint32_t* min_kb,
                                 float* fragmentation);

/**
 * @brief Power profile for BLE spam
 */
typedef enum {
  BT_SPAM_POWER_HIGH = 0,      // Max performance, max power
  BT_SPAM_POWER_BALANCED = 1,  // Balance between range and battery
  BT_SPAM_POWER_LOW = 2,       // Max battery life, reduced range
} bt_spam_power_profile_t;

/**
 * @brief Apply power profile (TX power + advertising interval)
 * @param profile Power profile to apply
 * @return ESP_OK on success
 */
esp_err_t bt_spam_set_power_profile(bt_spam_power_profile_t profile);

/**
 * @brief Get current power profile
 * @return Current power profile
 */
bt_spam_power_profile_t bt_spam_get_power_profile(void);

/**
 * @brief Set a chained GAP callback for other modules to receive BLE events
 * This allows multiple modules (e.g., spam + scanner) to share the single GAP
 * callback
 * @param cb Callback to chain (can be NULL to clear)
 */
void bt_spam_set_chained_gap_cb(esp_gap_ble_cb_t cb);

#endif  // BLE_SPAM_H
