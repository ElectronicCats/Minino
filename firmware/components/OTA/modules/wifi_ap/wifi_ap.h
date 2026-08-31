#pragma once

#include "esp_netif.h"
#include "wifi_ap_config.h"

// Network Interface Object for the Station and Access Point
extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

/*
 * Message ID's for WiFi Application Task
 */
typedef enum wifi_app_msg {
  WIFI_APP_MSG_START_HTTP_SERVER = 0,
  WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
  WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
} wifi_app_msg_e;

/*
 * Structure for Message Queue
 */
typedef struct wifi_app_q_msg {
  wifi_app_msg_e msg_id;
} wifi_app_q_msg_t;

// Public Function Prototypes
void wifi_app_start(void);
