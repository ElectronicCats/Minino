#include "esp_wifi.h"
#ifndef CAPTIVE_PORTAL_H
  #define CAPTIVE_PORTAL_H

typedef void (*captive_portal_handler_cb)(char* ssid, char* user, char* pass);

/**
 * @brief List to the names of portals
 *
 */
char* CAPTIVE_PORTALS_LIST[] = {"Google", "Wifi Pass", NULL};

typedef enum {
  CAPTIVE_PORTAL_GOOGLE = 0,
  CAPTIVE_PORTAL_WIFI_PASS,
} captive_portals_t;

void captive_portal_register_cb(captive_portal_handler_cb callback);
void captive_portal_set_config(wifi_config_t* config);
void captive_portal_set_portal(captive_portals_t portal);
void captive_portal_begin();
void captive_portal_stop();
#endif  // CAPTIVE_PORTAL_H
