
#ifndef SPAM_MODULE_H
#define SPAM_MODULE_H

#define TAG_BLE_MODULE "ble_module:main"

void ble_spam_apple_begin(void);
void ble_spam_android_begin(void);
void ble_spam_windows_begin(void);
void ble_spam_samsung_begin(void);
void ble_spam_all_begin(void);

/* Legacy alias for backwards compatibility */
void ble_module_begin(void);

#endif  // SPAM_MODULE_H
