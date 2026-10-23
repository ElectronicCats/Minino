#pragma once

#include "esp_wifi.h"

#define TAG_WIFI_DRIVER                 "wifi_controller:main"
#define WIFI_MANAGER_AP_SSID            CONFIG_MANAGER_AP_SSID
#define WIFI_MANAGER_AP_PASSWORD        CONFIG_MANAGER_AP_PASSWORD
#define WIFI_MANAGER_AP_MAX_CONNECTIONS CONFIG_MANAGER_AP_MAX_CONNECTIONS

wifi_config_t wifi_driver_access_point_begin(void);
void wifi_driver_ap_start(wifi_config_t* wifi_ap_config);
void wifi_driver_ap_stop(void);
/**
 * @brief Bring the Wi-Fi driver up
 *
 * All three return false when the driver could not be started: the radio
 * may be held by BLE, which leaves no room for the driver buffers, and
 * aborting the device is not an option for a caller that can report the
 * problem.
 */
bool wifi_driver_init_apsta(void);
bool wifi_driver_init_sta(void);
bool wifi_driver_init_null(void);
void wifi_driver_deinit();
void wifi_driver_deinit_if_started(void);
bool wifi_driver_get_initialized(void);
void wifi_driver_sta_disconnect();
void wifi_driver_set_ap_mac(const uint8_t* mac_ap);
void wifi_driver_get_ap_mac(uint8_t* mac_ap);
void wifi_driver_restore_ap_mac();
void wifi_driver_get_sta_mac(uint8_t* mac_sta);
void wifi_driver_set_channel(uint8_t channel);

/**
 * @brief Channel the Wi-Fi driver is on
 *
 * The driver starts on channel 1. Asking it for a channel it is already on
 * still restarts its radio, so a caller that only wants to be on a channel
 * can compare first.
 */
uint8_t wifi_driver_get_channel(void);

/**
 * @brief Record the channel the driver was put on
 *
 * For the callers that set the channel themselves (they need the driver's
 * error code) so that wifi_driver_get_channel() stays true.
 */
void wifi_driver_note_channel(uint8_t channel);
