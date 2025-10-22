#include "wifi_controller.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "string.h"

static bool wifi_driver_initialized = false;
static bool netif_default_created = false;
static bool run_once = false;
static uint8_t default_ap_mac[6];
static esp_err_t err;
static wifi_config_t wifi_manager_config;

wifi_config_t wifi_driver_access_point_begin() {
  // #if !defined(CONFIG_WIFI_CONTROLLER_DEBUG)
  //   esp_log_level_set(TAG_WIFI_DRIVER, ESP_LOG_NONE);
  // #endif

  if (wifi_driver_initialized) {
    wifi_driver_deinit();
  }

  esp_err_t err;
  err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error creating event loop: %s",
             esp_err_to_name(err));
    esp_restart();
  }

  wifi_config_t wifi_manager_config = {
      .ap = {.ssid = WIFI_MANAGER_AP_SSID,
             .ssid_len = strlen(WIFI_MANAGER_AP_SSID),
             .password = WIFI_MANAGER_AP_PASSWORD,
             .max_connection = WIFI_MANAGER_AP_MAX_CONNECTIONS,
             .authmode = WIFI_AUTH_WPA2_PSK},
  };
  wifi_driver_ap_start(&wifi_manager_config);
  return wifi_manager_config;
}

void wifi_driver_ap_start(wifi_config_t* wifi_ap_config) {
  ESP_LOGI(TAG_WIFI_DRIVER, "Starting WiFi Access Point %s",
           wifi_ap_config->ap.ssid);
  if (!wifi_driver_initialized) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Initializing WiFi Access Point and Station");
    wifi_driver_init_apsta();
  }

  err = esp_wifi_set_config(ESP_IF_WIFI_AP, wifi_ap_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER,
             "Error setting WiFi Access Point configuration: %s",
             esp_err_to_name(err));
    ESP_LOGI(TAG_WIFI_DRIVER, "%s", wifi_ap_config->ap.ssid);
    return;
  }
  ESP_LOGI(TAG_WIFI_DRIVER, "WiFi Access Point started SSID: %s",
           wifi_ap_config->ap.ssid);
}

void wifi_driver_ap_stop(void) {
  ESP_LOGI(TAG_WIFI_DRIVER, "Stopping WiFi Access Point");
  wifi_config_t wifi_ap_config = {
      .ap = {.max_connection = 0},
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));
  ESP_LOGI(TAG_WIFI_DRIVER, "WiFi Access Point stopped");
}

void wifi_driver_init_apsta(void) {
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error initializing netif: %s",
             esp_err_to_name(err));
  }

  // Create a defailt WiFi Access Point
  if (!netif_default_created) {
    // Create a default WiFi Station
    esp_netif_create_default_wifi_sta();

    netif_default_created = true;
  }

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  // This mode is used to connect to a WiFi network and create an Access Point
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  // Save the MAC address of the default Access Point
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, default_ap_mac));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_driver_initialized = true;
}

void wifi_driver_init_sta(void) {
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error initializing netif: %s",
             esp_err_to_name(err));
    esp_restart();
  }
  err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error creating event loop: %s",
             esp_err_to_name(err));
    esp_restart();
  }

  // This shouldn't be a definitive solution, but works for now

  if (!run_once) {
    run_once = true;
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_driver_initialized = true;
}

void wifi_driver_init_null(void) {
  esp_err_t err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error creating event loop: %s",
             esp_err_to_name(err));
    esp_restart();
  }
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  // ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
  wifi_driver_initialized = true;
}

void wifi_driver_deinit() {
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());
  wifi_driver_initialized = false;
}

void wifi_driver_sta_disconnect() {
  ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void wifi_driver_set_ap_mac(const uint8_t* mac_ap) {
  ESP_LOGD(TAG_WIFI_DRIVER, "Changing AP MAC address...");
  ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, mac_ap));
}

void wifi_driver_get_ap_mac(uint8_t* mac_ap) {
  esp_wifi_get_mac(WIFI_IF_AP, mac_ap);
}

void wifi_driver_restore_ap_mac() {
  ESP_LOGD(TAG_WIFI_DRIVER, "Restoring original AP MAC address...");
  err = esp_wifi_set_mac(WIFI_IF_AP, default_ap_mac);

  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Failed to restore AP MAC address: %s",
             esp_err_to_name(err));
  }
}

void wifi_driver_get_sta_mac(uint8_t* mac_sta) {
  esp_wifi_get_mac(WIFI_IF_STA, mac_sta);
}

void wifi_driver_set_channel(uint8_t channel) {
  if ((channel == 0) || (channel > 13)) {
    ESP_LOGE(TAG_WIFI_DRIVER,
             "Channel out of range. Expected value from <1,13> but got %u",
             channel);
    return;
  }
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}
