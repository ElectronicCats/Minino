/*
 * wifi_app.h
 *
 *  Created on: 15-Jul-2023
 *      Author: xpress_embedo
 */

#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include "esp_netif.h"

// Macros
// WiFi Application Settings
#define WIFI_AP_SSID            "MININO_AP"  // Access Point Names
#define WIFI_AP_PASSWORD        "Cats1234"   // Access Point Password
#define WIFI_AP_CHANNEL         1            // Access Point Channels
#define WIFI_AP_SSID_HIDDEN     0            // Access Point Visibility
#define WIFI_AP_MAX_CONNECTIONS 5            // Access Point Max Clients
#define WIFI_AP_BEACON_INTERVAL \
  100  // Access Point Beacon Interval set to 100ms as recommended
#define WIFI_AP_IP            "192.168.0.1"  // Default IP
#define WIFI_AP_GATEWAY       "192.168.0.1"  // Default Gateway (should be same as IP)
#define WIFI_AP_NETMASK       "255.255.255.0"  // AP NetMask
#define WIFI_AP_BANDWIDTH     WIFI_BW_HT20     // AP bandwidth 20MHz
#define WIFI_STA_POWER_SAVE   WIFI_PS_NONE     // No Power Saving
#define WIFI_MAX_SSID_LEN     32               // IEEE Standard Maximum
#define WIFI_MAX_PASSWORD_LEN 64               // IEEE Standard Maximum
#define WIFI_MAX_CONN_RETRIES 5                // Retry Number of Disconnect

// Network Interface Object for th Station and Access Point
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

#endif /* MAIN_WIFI_APP_H_ */
