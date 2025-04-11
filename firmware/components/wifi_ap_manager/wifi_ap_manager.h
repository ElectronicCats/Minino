#ifndef __WIFI_AP_MANAGER_H__
#define __WIFI_AP_MANAGER_H__

#include <stdbool.h>

#define JOIN_TIMEOUT_MS         (5000)
#define MAXIMUM_RETRY           (3)
#define WIFI_AP_SSID            "MININO_AP"  // MAX 32
#define WIFI_AP_PASSWORD        "Cats1234"   // MAX 64
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_SSID_HIDDEN     0
#define WIFI_AP_MAX_CONNECTIONS 5
#define WIFI_AP_BEACON_INTERVAL 100
#define WIFI_AP_IP              "192.168.0.1"
#define WIFI_AP_GATEWAY         "192.168.0.1"
#define WIFI_AP_NETMASK         "255.255.255.0"
#define WIFI_AP_BANDWIDTH       WIFI_BW_HT20
#define WIFI_STA_POWER_SAVE     WIFI_PS_NONE
#define WIFI_AUTH_MODE          WIFI_AUTH_WPA2_PSK

typedef void (*app_callback)(bool state);

int wifi_ap_manager_connect_ap(const char* ssid, const char* pass);
int wifi_ap_manager_connect_index(int index);
void wifi_ap_manager_unregister_callback();
int wifi_ap_manager_connect_index_cb(int index, app_callback cb);
int wifi_ap_manager_delete_ap_by_index(int index);

#define WIFI_AP_CONFIG()                          \
  {                                               \
    .ap = {                                       \
      .ssid = WIFI_AP_SSID,                       \
      .ssid_len = strlen(WIFI_AP_SSID),           \
      .password = WIFI_AP_PASSWORD,               \
      .max_connection = WIFI_AP_MAX_CONNECTIONS,  \
      .channel = WIFI_AP_CHANNEL,                 \
      .ssid_hidden = WIFI_AP_SSID_HIDDEN,         \
      .authmode = WIFI_AUTH_MODE,                 \
      .beacon_interval = WIFI_AP_BEACON_INTERVAL, \
    }                                             \
  }

#endif