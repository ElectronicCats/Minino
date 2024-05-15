#include "esp_wifi.h"
#include "keyboard_module.h"
#include "menu_screens_modules.h"

#ifndef WIFI_MODULE_H
  #define WIFI_MODULE_H
  #define TAG_WIFI_MODULE "module:wifi"

/**
 * @brief Enum with the wifi module states
 *
 */
typedef enum {
  WIFI_STATE_SCANNING = 0,
  WIFI_STATE_SCANNED,
  WIFI_STATE_DETAILS,
  WIFI_STATE_ATTACK_SELECTOR,
  WIFI_STATE_ATTACK,
  WIFI_STATE_ATTACK_CAPTIVE_PORTAL,
} wifi_state_t;

/**
 * @brief Structure to store the wifi module data
 *
 */
typedef struct {
  wifi_state_t state;
  wifi_config_t wifi_config;
} wifi_module_t;

char* wifi_state_names[] = {
    "WIFI_STATE_SCANNING", "WIFI_STATE_SCANNED",
    "WIFI_STATE_DETAILS",  "WIFI_STATE_ATTACK_SELECTOR",
    "WIFI_STATE_ATTACK",   "WIFI_STATE_ATTACK_CAPTIVE_PORTAL",
};

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
void wifi_module_state_machine(button_event_t button_pressed);
#endif  // WIFI_MODULE_H
