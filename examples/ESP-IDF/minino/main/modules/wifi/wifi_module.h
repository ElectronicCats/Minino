#include "esp_wifi.h"
#include "keyboard_module.h"
#include "menu_screens_modules.h"

#ifndef WIFI_MODULE_H
  #define WIFI_MODULE_H
  #define TAG_WIFI_MODULE "module:wifi"

/**
 * @brief Start the wifi module
 *
 */
void wifi_module_begin(void);

/**
 * @brief Stop the wifi module
 *
 */
void wifi_module_exit(void);

/**
 * @brief State machine for the wifi module
 *
 * @param button_pressed The button pressed
 */
void wifi_module_keyboard_cb(button_event_t button_pressed);
#endif  // WIFI_MODULE_H
