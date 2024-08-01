#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"
#include "nvs_flash.h"

#include "wifi_app.h"

esp_netif_t* esp_netif_ap = NULL;

void wifi_ap_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  esp_log_level_set("wifi", ESP_LOG_NONE);

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK) {
    ESP_LOGE(__func__, "Error creating event loop");
    return;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  esp_netif_ap = esp_netif_create_default_wifi_ap();

  wifi_config_t ap_config = WIFI_AP_CONFIG();

  esp_netif_ip_info_t ap_ip_info;
  memset(&ap_ip_info, 0, sizeof(ap_ip_info));
  esp_netif_dhcps_stop(esp_netif_ap);
  inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);
  inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
  inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);
  ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
  ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));

  ESP_ERROR_CHECK(esp_wifi_start());
}
