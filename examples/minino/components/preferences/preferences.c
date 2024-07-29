#include "preferences.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdlib.h"

static const char* TAG = "preferences";
esp_err_t _return_err;
nvs_handle_t _nvs_handler;
bool _started;
bool _read_only;

void _check_started() {
  if (!_started) {
    ESP_LOGE(TAG, "Preferences not started! Call preferences_begin() first!");
    abort();
  }
}

esp_err_t _commit() {
  if (_return_err) {
    ESP_LOGE(TAG, "Error (%s) writing value!", esp_err_to_name(_return_err));
    return _return_err;
  }

  _return_err = nvs_commit(_nvs_handler);
  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) committing value!", esp_err_to_name(_return_err));
    return _return_err;
  }

  return ESP_OK;
}

esp_err_t preferences_begin() {
#if !defined(CONFIG_PREFERENCES_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  // Initialize NVS
  _return_err = nvs_flash_init();
  if (_return_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      _return_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    _return_err = nvs_flash_init();
  }

  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) initializing NVS!", esp_err_to_name(_return_err));
    return _return_err;
  }

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle...");
  _started = true;
  _return_err = nvs_open("storage", _read_only ? NVS_READONLY : NVS_READWRITE,
                         &_nvs_handler);

  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS!", esp_err_to_name(_return_err));
    return _return_err;
  }

  return ESP_OK;
}

void preferences_end() {
  ESP_LOGI(TAG, "Closing Non-Volatile Storage (NVS) handle...");
  nvs_close(_nvs_handler);
  _started = false;
}

esp_err_t preferences_clear() {
  ESP_LOGI(TAG, "Clearing NVS...");
  _return_err = nvs_erase_all(_nvs_handler);
  return _return_err;
}

esp_err_t preferences_remove(const char* key) {
  if (!_started) {
    preferences_begin();
  }

  _return_err = nvs_erase_key(_nvs_handler, key);
  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) removing key!", esp_err_to_name(_return_err));
    return _return_err;
  }

  _return_err = nvs_commit(_nvs_handler);
  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) committing key removal!",
             esp_err_to_name(_return_err));
    return _return_err;
  }

  return ESP_OK;
}

esp_err_t preferences_put_char(const char* key, int8_t value) {
  _check_started();
  _return_err = nvs_set_i8(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_uchar(const char* key, uint8_t value) {
  _check_started();
  _return_err = nvs_set_u8(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_short(const char* key, int16_t value) {
  _check_started();
  _return_err = nvs_set_i16(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_ushort(const char* key, uint16_t value) {
  _check_started();
  _return_err = nvs_set_u16(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_int(const char* key, int32_t value) {
  _check_started();
  _return_err = nvs_set_i32(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_uint(const char* key, uint32_t value) {
  _check_started();
  _return_err = nvs_set_u32(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_long(const char* key, int32_t value) {
  _check_started();
  _return_err = nvs_set_i64(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_ulong(const char* key, uint32_t value) {
  _check_started();
  _return_err = nvs_set_u64(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_long64(const char* key, int64_t value) {
  _check_started();
  _return_err = nvs_set_i64(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_ulong64(const char* key, uint64_t value) {
  _check_started();
  _return_err = nvs_set_u64(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_float(const char* key, float value) {
  return preferences_put_bytes(key, (void*) &value, sizeof(float));
}

esp_err_t preferences_put_double(const char* key, double value) {
  return preferences_put_bytes(key, (void*) &value, sizeof(double));
}

esp_err_t preferences_put_bool(const char* key, bool value) {
  _check_started();
  _return_err = nvs_set_u8(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_string(const char* key, const char* value) {
  _check_started();
  _return_err = nvs_set_str(_nvs_handler, key, value);
  return _commit();
}

esp_err_t preferences_put_bytes(const char* key,
                                const void* value,
                                size_t length) {
  _check_started();
  _return_err = nvs_set_blob(_nvs_handler, key, value, length);
  return _commit();
}

int8_t preferences_get_char(const char* key, int8_t default_value) {
  _check_started();

  int8_t value;
  _return_err = nvs_get_i8(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

uint8_t preferences_get_uchar(const char* key, uint8_t default_value) {
  _check_started();

  uint8_t value;
  _return_err = nvs_get_u8(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

int16_t preferences_get_short(const char* key, int16_t default_value) {
  _check_started();

  int16_t value;
  _return_err = nvs_get_i16(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

uint16_t preferences_get_ushort(const char* key, uint16_t default_value) {
  _check_started();

  uint16_t value;
  _return_err = nvs_get_u16(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

int32_t preferences_get_int(const char* key, int32_t default_value) {
  _check_started();

  int32_t value;
  _return_err = nvs_get_i32(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

uint32_t preferences_get_uint(const char* key, uint32_t default_value) {
  _check_started();

  uint32_t value;
  _return_err = nvs_get_u32(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

int32_t preferences_get_long(const char* key, int32_t default_value) {
  _check_started();

  int32_t value;
  _return_err = nvs_get_i32(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

uint32_t preferences_get_ulong(const char* key, uint32_t default_value) {
  _check_started();

  uint32_t value;
  _return_err = nvs_get_u32(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

int64_t preferences_get_long64(const char* key, int64_t default_value) {
  _check_started();

  int64_t value;
  _return_err = nvs_get_i64(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

uint64_t preferences_get_ulong64(const char* key, uint64_t default_value) {
  _check_started();

  uint64_t value;
  _return_err = nvs_get_u64(_nvs_handler, key, &value);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return default_value;
  }

  return value;
}

float preferences_get_float(const char* key, float default_value) {
  float value = default_value;
  preferences_get_bytes(key, &value, sizeof(float));
  return value;
}

double preferences_get_double(const char* key, double default_value) {
  double value = default_value;
  preferences_get_bytes(key, &value, sizeof(double));
  return value;
}

bool preferences_get_bool(const char* key, bool default_value) {
  return preferences_get_uchar(key, default_value) == 1;
}

// TODO: this is not working
esp_err_t preferences_get_string(const char* key,
                                 char* value,
                                 size_t max_length) {
  _check_started();
  size_t length = 0;

  _return_err = nvs_get_str(_nvs_handler, key, NULL, &length);

  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return ESP_ERR_NVS_NOT_FOUND;
  }

  if (length > max_length) {
    ESP_LOGE(TAG, "The value is too long for the buffer!");
    return ESP_ERR_NVS_INVALID_LENGTH;
  }

  _return_err = nvs_get_str(_nvs_handler, key, value, &length);
  return _return_err;
}

size_t preferences_get_bytes_length(const char* key) {
  _check_started();

  size_t length;
  _return_err = nvs_get_blob(_nvs_handler, key, NULL, &length);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return 0;
  }

  return length;
}

esp_err_t preferences_get_bytes(const char* key, void* buffer, size_t length) {
  _check_started();

  _return_err = nvs_get_blob(_nvs_handler, key, buffer, &length);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return ESP_ERR_NVS_NOT_FOUND;
  }

  return _return_err;
}
