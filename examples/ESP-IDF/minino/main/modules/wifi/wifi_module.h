#include "esp_wifi.h"
#include "keyboard_module.h"
#include "menu_screens_modules.h"

#ifndef WIFI_MODULE_H
  #define WIFI_MODULE_H
  #define TAG_WIFI_MODULE "module:wifi"

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_deauth_begin();

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_analizer_begin();

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