#include "wifi_controller.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "string.h"
#include "wifi_service.h"

static uint8_t default_ap_mac[6];
static esp_err_t err;
static uint8_t s_current_channel = 1;

/* Up and started, read from the driver every time. A cached flag
   went stale whenever an app called wifi_service_deinit() or esp_wifi_stop()
   on its own, and the next init then skipped esp_wifi_start(). The tx-power
   getter is read-only and documented to fail with ESP_ERR_WIFI_NOT_INIT or
   ESP_ERR_WIFI_NOT_STARTED. */
static bool wifi_driver_started(void) {
  int8_t power;
  return wifi_service_is_up_get() &&
         esp_wifi_get_max_tx_power(&power) == ESP_OK;
}

wifi_config_t wifi_driver_access_point_begin() {
  // #if !defined(CONFIG_WIFI_CONTROLLER_DEBUG)
  //   esp_log_level_set(TAG_WIFI_DRIVER, ESP_LOG_NONE);
  // #endif

  /* The driver is brought up (and torn down) by the caller's radio_lease
     claim; wifi_driver_ap_start() below only sets the AP config when it is
     already running. Deinitialising here would drop the lease's driver and
     leave the refcount pointing at nothing (H7). */

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
  if (!wifi_driver_started()) {
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

bool wifi_driver_init_apsta(void) {
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error initializing netif: %s",
             esp_err_to_name(err));
  }

  wifi_service_netif_sta();

  if (!wifi_service_is_up_get() && !wifi_service_init(WIFI_MODE_APSTA)) {
    ESP_LOGE(TAG_WIFI_DRIVER, "WiFi stack init failed");
    return false;
  }
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
      wifi_service_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
      esp_wifi_get_mac(WIFI_IF_AP, default_ap_mac) != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "could not set the driver up in APSTA mode");
    return false;
  }
  // Save the MAC address of the default Access Point
  if (esp_wifi_start() != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "esp_wifi_start failed");
    return false;
  }
  return true;
}

bool wifi_driver_init_sta(void) {
  if (wifi_driver_started()) {
    /* Setting the mode the driver already has restarts its radio, and the
       radio's buffers are 37 KB of heap: only ask when the mode really is a
       different one. */
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_STA) {
      return true;
    }
    return wifi_service_set_mode(WIFI_MODE_STA) == ESP_OK;
  }

  esp_err_t err = esp_netif_init();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "netif already initialized");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error initializing netif: %s",
             esp_err_to_name(err));
    // Do not reboot - keep trying
  }
  err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Event loop already created");
  }
  wifi_service_netif_sta();

  if (!wifi_service_is_up_get() && !wifi_service_init(WIFI_MODE_APSTA)) {
    ESP_LOGE(TAG_WIFI_DRIVER, "WiFi stack init failed");
    return false;
  }
  if (wifi_service_set_mode(WIFI_MODE_STA) != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "wifi_service_set_mode(STA) failed");
    return false;
  }
  if (esp_wifi_start() != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER,
             "esp_wifi_start failed: the radio is probably held by BLE");
    return false;
  }
  return true;
}

bool wifi_driver_init_null(void) {
  if (wifi_driver_started()) {
    return wifi_service_set_mode(WIFI_MODE_NULL) == ESP_OK;
  }
  esp_err_t err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG_WIFI_DRIVER, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "Error creating event loop: %s",
             esp_err_to_name(err));
    esp_restart();
  }
  if (!wifi_service_is_up_get() && !wifi_service_init(WIFI_MODE_APSTA)) {
    ESP_LOGE(TAG_WIFI_DRIVER, "WiFi stack init failed");
    return false;
  }
  if (wifi_service_set_mode(WIFI_MODE_NULL) != ESP_OK) {
    ESP_LOGE(TAG_WIFI_DRIVER, "wifi_service_set_mode(NULL) failed");
    return false;
  }
  return true;
}

void wifi_driver_deinit() {
  esp_wifi_stop();
  wifi_service_deinit();
  s_current_channel = 1;
}

// Releases the 2.4 GHz radio for other stacks (e.g. pure BLE) without assuming
// Wi-Fi was started: calling deinit() directly would abort() if it was never
// initialized, because ESP_ERROR_CHECK does not forgive ESP_ERR_WIFI_NOT_INIT.
void wifi_driver_deinit_if_started(void) {
  esp_wifi_stop();
  wifi_service_deinit();
  s_current_channel = 1;
}

bool wifi_driver_get_initialized(void) {
  return wifi_driver_started();
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

/* The channel the driver is on. The driver starts on channel 1 and only
   changes when it is told to; the state is kept here so that a caller can
   tell whether a change is needed at all: asking the driver for the channel
   it is already on still restarts its radio, and the radio's capture
   buffers cost 40 KB of heap. */

void wifi_driver_set_channel(uint8_t channel) {
  if ((channel == 0) || (channel > 13)) {
    ESP_LOGE(TAG_WIFI_DRIVER,
             "Channel out of range. Expected value from <1,13> but got %u",
             channel);
    return;
  }
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  s_current_channel = channel;
}

uint8_t wifi_driver_get_channel(void) {
  /* Ask the driver first: it may have been re-initialised (back on channel
     1) or moved by code outside this module. The read does not
     touch the radio. */
  uint8_t primary = 0;
  wifi_second_chan_t second;
  if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary >= 1 &&
      primary <= 13) {
    s_current_channel = primary;
  }
  return s_current_channel;
}

void wifi_driver_note_channel(uint8_t channel) {
  if ((channel >= 1) && (channel <= 13)) {
    s_current_channel = channel;
  }
}
