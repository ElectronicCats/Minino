#pragma once

#include "esp_netif.h"

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

void wifi_ap_init(void);