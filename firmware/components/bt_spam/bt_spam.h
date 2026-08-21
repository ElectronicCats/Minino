
#ifndef BLE_SPAM_H
#define BLE_SPAM_H

#include <stdbool.h>
#include <stdint.h>

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

#endif  // BLE_SPAM_H
