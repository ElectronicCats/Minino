#ifndef WIFI_REGIONS_H
#define WIFI_REGIONS_H

#include "esp_wifi.h"

#define WIFI_REGION_MEM "wifi_region"

#define WIFI_REGION_AMERICA()                                \
  (wifi_country_t) {                                         \
    .cc = "US", .schan = 1, .nchan = 11, .max_tx_power = 20, \
    .policy = WIFI_COUNTRY_POLICY_AUTO                       \
  }

#define WIFI_REGION_ASIA_JAPAN()                             \
  (wifi_country_t) {                                         \
    .cc = "JP", .schan = 1, .nchan = 14, .max_tx_power = 20, \
    .policy = WIFI_COUNTRY_POLICY_AUTO                       \
  }

#define WIFI_REGION_ASIA_CHINA()                             \
  (wifi_country_t) {                                         \
    .cc = "CN", .schan = 1, .nchan = 13, .max_tx_power = 20, \
    .policy = WIFI_COUNTRY_POLICY_AUTO                       \
  }

#define WIFI_REGION_EUROPA()                                 \
  (wifi_country_t) {                                         \
    .cc = "DE", .schan = 1, .nchan = 13, .max_tx_power = 20, \
    .policy = WIFI_COUNTRY_POLICY_AUTO                       \
  }

#define WIFI_REGION_GLOBAL()                                 \
  (wifi_country_t) {                                         \
    .cc = "01", .schan = 1, .nchan = 11, .max_tx_power = 20, \
    .policy = WIFI_COUNTRY_POLICY_AUTO                       \
  }

esp_err_t wifi_regions_set_country();

#endif  // WIFI_REGIONS_H
