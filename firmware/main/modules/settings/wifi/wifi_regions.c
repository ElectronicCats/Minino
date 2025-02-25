#include "wifi_regions.h"

#include "esp_err.h"
#include "esp_log.h"

#include "preferences.h"
#include "wifi_controller.h"

#define TAG "WIFI REGIONS"

typedef enum {
  GLOBAL_REGION,
  AMERICA_REGION,
  EUROPA_REGION,
  ASIA_REGION,
  JAPAN_REGION
} wifi_regions_e;

esp_err_t wifi_regions_set_country() {
  wifi_driver_init_null();
  esp_err_t err = ESP_OK;
  wifi_country_t country;
  uint8_t country_idx = preferences_get_uchar(WIFI_REGION_MEM, 0);

  switch (country_idx) {
    case AMERICA_REGION:
      country = WIFI_REGION_AMERICA();
      break;
    case EUROPA_REGION:
      country = WIFI_REGION_EUROPA();
      break;
    case ASIA_REGION:
      country = WIFI_REGION_ASIA_CHINA();
      break;
    case JAPAN_REGION:
      country = WIFI_REGION_ASIA_JAPAN();
      break;
    default:
      country = WIFI_REGION_GLOBAL();
      break;
  }
  err = esp_wifi_set_country(&country);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting WiFi Country code configuration: %s",
             esp_err_to_name(err));
    return err;
  }

  return err;
}