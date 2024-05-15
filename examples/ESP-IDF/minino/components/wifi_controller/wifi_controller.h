#include "esp_wifi.h"
#ifndef WIFI_DRIVER_H
  #define WIFI_DRIVER_H
  #define TAG_WIFI_DRIVER                 "wifi_controllera:main"
  #define WIFI_MANAGER_AP_SSID            CONFIG_MANAGER_AP_SSID
  #define WIFI_MANAGER_AP_PASSWORD        CONFIG_MANAGER_AP_PASSWORD
  #define WIFI_MANAGER_AP_MAX_CONNECTIONS CONFIG_MANAGER_AP_MAX_CONNECTIONS

wifi_config_t wifi_driver_access_point_begin(void);
void wifi_driver_ap_start(wifi_config_t* wifi_ap_config);
void wifi_driver_ap_stop(void);
void wifi_driver_init_apsta(void);
void wifi_driver_sta_disconnect();
void wifi_driver_set_ap_mac(const uint8_t* mac_ap);
void wifi_driver_get_ap_mac(uint8_t* mac_ap);
void wifi_driver_restore_ap_mac();
void wifi_driver_get_sta_mac(uint8_t* mac_sta);
void wifi_driver_set_channel(uint8_t channel);
#endif  // WIFI_DRIVER_H
