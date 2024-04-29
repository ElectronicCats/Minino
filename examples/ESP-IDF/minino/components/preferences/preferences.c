#include "preferences.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdlib.h"

static const char* TAG = "preferences";
esp_err_t return_err;
nvs_handle_t _nvs_handler;
bool _started;
bool _read_only;

size_t preferences_begin() {
  // Initialize NVS
  return_err = nvs_flash_init();
  if (return_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      return_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    return_err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(return_err);

  _started = true;

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle...");
  return_err = nvs_open("storage", _read_only ? NVS_READONLY : NVS_READWRITE,
                        &_nvs_handler);
  ESP_ERROR_CHECK(return_err);
  return return_err;
}

size_t preferences_end() {
  ESP_LOGI(TAG, "Closing Non-Volatile Storage (NVS) handle...");
  nvs_close(_nvs_handler);
  _started = false;
  return ESP_OK;
}

size_t preferences_clear() {
  ESP_LOGI(TAG, "Clearing NVS...");
  return_err = nvs_erase_all(_nvs_handler);
  ESP_ERROR_CHECK(return_err);
  return return_err;
}

size_t preferences_remove() {
  ESP_LOGI(TAG, "Removing NVS...");
  return_err = nvs_flash_deinit();
  ESP_ERROR_CHECK(return_err);
  return return_err;
}

size_t preferences_put_int(const char* key, int32_t value) {
  if (!_started) {
    preferences_begin();
  }

  // ESP_LOGI(TAG, "Setting %s to %d...", key, value);
  return_err = nvs_set_i32(_nvs_handler, key, value);
  ESP_ERROR_CHECK(return_err);
  return return_err;
}

int32_t preferences_get_int(const char* key, int32_t default_value) {
  // ESP_LOGI(TAG, "Getting %s...", key);
  int32_t value;
  return_err = nvs_get_i32(_nvs_handler, key, &value);
  if (return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "The value is not initialized yet!");
    return default_value;
  }
  ESP_ERROR_CHECK(return_err);
  return value;
}
