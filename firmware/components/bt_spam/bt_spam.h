
#ifndef BLE_SPAM_H
#define BLE_SPAM_H
#define TAG_BLE_SPAM "ble_spam:main"
typedef void (*bt_spam_cb_display)(char* name);

/**
 * @brief Initialize the bluetooth spam module
 *
 */

void bt_spam_app_main();

/**
 * @brief Register the callback to display the spam data
 *
 * @param callback The callback to display the spam data
 */
void bt_spam_register_cb(bt_spam_cb_display callback);

void bt_spam_app_stop();
#endif  // BLE_SPAM_H
