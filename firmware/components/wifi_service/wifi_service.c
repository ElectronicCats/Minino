/* WiFi Service implementation.
 *
 * Key patterns:
 *   - Single initialization at boot (main -> wifi_service_init).
 *   - State protected by its own spinlock.
 *   - No event bus: each app uses direct callbacks or queries the state.
 *
 * Why a singleton:
 *   - Eliminates 13 esp_wifi_init() calls across the tree.
 *   - Single place for driver buffer configuration.
 *   - Mode getters query the driver directly, so apps that still call
 *     esp_wifi_set_mode() do not leave the service state stale.
 */

#include "wifi_service.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "wifi_svc";

/* Dedicated spinlock: protects s_state. */
static portMUX_TYPE s_wifi_mux = portMUX_INITIALIZER_UNLOCKED;

/* Volatile state, visible only through accessors (under s_wifi_mux). */
static wifi_service_state_t s_state = {
    .mode = WIFI_MODE_NULL,
    .iface = WIFI_IF_STA, /* default: station */
    .is_up = false,
    .is_sta = false,
    .is_ap = false,
};

static wifi_service_init_guard_t s_init_guard = NULL;

void wifi_service_set_init_guard(wifi_service_init_guard_t guard) {
  s_init_guard = guard;
}

/* ----------------------------------------------------------- Public API */

bool wifi_service_init(wifi_mode_t mode) {
  if (s_init_guard != NULL && !s_init_guard()) {
    ESP_LOGW(TAG, "WiFi bring-up vetoed: BLE owns the radio");
    return false;
  }

  portENTER_CRITICAL(&s_wifi_mux);
  if (s_state.is_up) {
    portEXIT_CRITICAL(&s_wifi_mux);
    ESP_LOGW(TAG, "already initialized");
    return true;
  }

  /* Default mode: station. Apps that need AP request it via
     wifi_service_set_mode(); there are no CONFIG_STA_ENABLE/AP_ENABLE symbols.
   */
  if (mode == WIFI_MODE_AP || mode == WIFI_MODE_STA ||
      mode == WIFI_MODE_APSTA || mode == WIFI_MODE_NULL) {
    s_state.mode = mode;
  } else {
    s_state.mode = WIFI_MODE_STA;
  }
  s_state.is_sta =
      (s_state.mode == WIFI_MODE_STA || s_state.mode == WIFI_MODE_APSTA);
  s_state.is_ap =
      (s_state.mode == WIFI_MODE_AP || s_state.mode == WIFI_MODE_APSTA);
  portEXIT_CRITICAL(&s_wifi_mux);

  /* Initialize the WiFi stack outside the critical section */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  /* The capture bank takes 128 KB of SRAM, so the Wi-Fi driver gets a
     smaller share of it: every buffer here is heap the rest of the system
     cannot use. */
  cfg.static_rx_buf_num = 2;
  cfg.dynamic_rx_buf_num = 4;
  cfg.dynamic_tx_buf_num = 4;
  /* The driver keeps a cookie with the calibration data in the NVS
     partition, and it opens it with the partition already open the second
     time round (the Bluetooth brings the driver down and up again): the
     second esp_wifi_init() then fails with "init nvs: failed, ret=101"
     and the Wi-Fi apps do not start. Without the cookie the driver
     recalibrates on every boot, which is a few hundred milliseconds. */
  cfg.nvs_enable = false;
  esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
    return false;
  }

  portENTER_CRITICAL(&s_wifi_mux);
  s_state.is_up = true;
  portEXIT_CRITICAL(&s_wifi_mux);

  ESP_LOGI(TAG, "initialized in %s mode",
           s_state.mode == WIFI_MODE_STA ? "STA" : "AP");
  return true;
}

bool wifi_service_deinit(void) {
  portENTER_CRITICAL(&s_wifi_mux);
  if (!s_state.is_up) {
    portEXIT_CRITICAL(&s_wifi_mux);
    return true; /* already deactivated */
  }
  s_state = (wifi_service_state_t) {0};
  s_state.is_up = false;
  portEXIT_CRITICAL(&s_wifi_mux);

  esp_wifi_stop();
  esp_err_t err = esp_wifi_deinit();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGW(TAG, "esp_wifi_deinit: %s", esp_err_to_name(err));
  }
  ESP_LOGI(TAG, "deinitialized");
  return true;
}

esp_err_t wifi_service_set_mode(wifi_mode_t mode) {
  if (mode != WIFI_MODE_NULL && mode != WIFI_MODE_STA && mode != WIFI_MODE_AP &&
      mode != WIFI_MODE_APSTA) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!wifi_service_is_up_get()) {
    return ESP_ERR_INVALID_STATE;
  }
  /* esp_wifi_set_mode requires WiFi to be stopped when changing between
     STA and AP modes. Stop first, then set mode, then restart. */
  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK) {
    return err;
  }
  err = esp_wifi_set_mode(mode);
  if (err != ESP_OK) {
    return err;
  }
  err = esp_wifi_start();
  if (err != ESP_OK) {
    return err;
  }
  portENTER_CRITICAL(&s_wifi_mux);
  s_state.mode = mode;
  s_state.is_sta = (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA);
  s_state.is_ap = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  portEXIT_CRITICAL(&s_wifi_mux);
  return ESP_OK;
}

wifi_mode_t wifi_service_mode_get(void) {
  if (!wifi_service_is_up_get()) {
    return WIFI_MODE_NULL;
  }
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) == ESP_OK) {
    return mode;
  }
  wifi_mode_t out;
  portENTER_CRITICAL(&s_wifi_mux);
  out = s_state.mode;
  portEXIT_CRITICAL(&s_wifi_mux);
  return out;
}

wifi_interface_t wifi_service_iface_get(void) {
  wifi_interface_t out;
  portENTER_CRITICAL(&s_wifi_mux);
  out = s_state.iface;
  portEXIT_CRITICAL(&s_wifi_mux);
  return out;
}

bool wifi_service_is_up_get(void) {
  bool out;
  portENTER_CRITICAL(&s_wifi_mux);
  out = s_state.is_up;
  portEXIT_CRITICAL(&s_wifi_mux);
  return out;
}

bool wifi_service_is_sta_get(void) {
  wifi_mode_t mode = wifi_service_mode_get();
  return mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA;
}

bool wifi_service_is_ap_get(void) {
  wifi_mode_t mode = wifi_service_mode_get();
  return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
}

wifi_second_chan_t wifi_service_primary_chan_get(void) {
  wifi_second_chan_t out;
  portENTER_CRITICAL(&s_wifi_mux);
  out = s_state.primary_chan;
  portEXIT_CRITICAL(&s_wifi_mux);
  return out;
}

wifi_second_chan_t wifi_service_secondary_chan_get(void) {
  wifi_second_chan_t out;
  portENTER_CRITICAL(&s_wifi_mux);
  out = s_state.secondary_chan;
  portEXIT_CRITICAL(&s_wifi_mux);
  return out;
}