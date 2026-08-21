#include "preferences.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdlib.h"

static const char* TAG = "preferences";
static esp_err_t _return_err = ESP_OK;
static nvs_handle_t _nvs_handler = 0;
static bool _started = false;
static bool _read_only = false;
static SemaphoreHandle_t _pref_mutex = NULL;

static bool _ensure_mutex(void) {
  if (_pref_mutex == NULL) {
    _pref_mutex = xSemaphoreCreateMutex();
  }
  return (_pref_mutex != NULL);
}

static bool _check_started(void) {
  if (!_started) {
    ESP_LOGW(TAG, "Preferences not started! Call preferences_begin() first!");
    return false;
  }
  return true;
}

static esp_err_t _commit(void) {
  if (_return_err != ESP_OK) {
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

  if (!_ensure_mutex()) {
    return ESP_ERR_NO_MEM;
  }

  if (xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (_started) {
    xSemaphoreGive(_pref_mutex);
    return ESP_OK;
  }

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
    xSemaphoreGive(_pref_mutex);
    return _return_err;
  }

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle...");
  _return_err = nvs_open("storage", _read_only ? NVS_READONLY : NVS_READWRITE,
                         &_nvs_handler);

  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS!", esp_err_to_name(_return_err));
    xSemaphoreGive(_pref_mutex);
    return _return_err;
  }

  _started = true;
  xSemaphoreGive(_pref_mutex);
  return ESP_OK;
}

void preferences_end() {
  if (_ensure_mutex() && xSemaphoreTake(_pref_mutex, portMAX_DELAY) == pdTRUE) {
    if (_started) {
      ESP_LOGI(TAG, "Closing Non-Volatile Storage (NVS) handle...");
      nvs_close(_nvs_handler);
      _started = false;
    }
    xSemaphoreGive(_pref_mutex);
  }
}

esp_err_t preferences_clear() {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_check_started()) {
    xSemaphoreGive(_pref_mutex);
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Clearing NVS...");
  _return_err = nvs_erase_all(_nvs_handler);
  esp_err_t res = (_return_err == ESP_OK) ? _commit() : _return_err;
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_remove(const char* key) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_started) {
    xSemaphoreGive(_pref_mutex);
    preferences_begin();
    if (xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) {
      return ESP_ERR_INVALID_STATE;
    }
  }

  _return_err = nvs_erase_key(_nvs_handler, key);
  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) removing key!", esp_err_to_name(_return_err));
    xSemaphoreGive(_pref_mutex);
    return _return_err;
  }

  _return_err = nvs_commit(_nvs_handler);
  if (_return_err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) committing key removal!",
             esp_err_to_name(_return_err));
    xSemaphoreGive(_pref_mutex);
    return _return_err;
  }

  xSemaphoreGive(_pref_mutex);
  return ESP_OK;
}

esp_err_t preferences_put_char(const char* key, int8_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_i8(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_uchar(const char* key, uint8_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u8(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_short(const char* key, int16_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_i16(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_ushort(const char* key, uint16_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u16(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_int(const char* key, int32_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_i32(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_uint(const char* key, uint32_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u32(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_long(const char* key, int32_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_i32(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_ulong(const char* key, uint32_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u32(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_long64(const char* key, int64_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_i64(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_ulong64(const char* key, uint64_t value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u64(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_float(const char* key, float value) {
  return preferences_put_bytes(key, (void*) &value, sizeof(float));
}

esp_err_t preferences_put_double(const char* key, double value) {
  return preferences_put_bytes(key, (void*) &value, sizeof(double));
}

esp_err_t preferences_put_bool(const char* key, bool value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_u8(_nvs_handler, key, value ? 1 : 0);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_string(const char* key, const char* value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_str(_nvs_handler, key, value);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

esp_err_t preferences_put_bytes(const char* key,
                                const void* value,
                                size_t length) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }
  _return_err = nvs_set_blob(_nvs_handler, key, value, length);
  esp_err_t res = _commit();
  xSemaphoreGive(_pref_mutex);
  return res;
}

int8_t preferences_get_char(const char* key, int8_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  int8_t value = default_value;
  _return_err = nvs_get_i8(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

uint8_t preferences_get_uchar(const char* key, uint8_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  uint8_t value = default_value;
  _return_err = nvs_get_u8(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

int16_t preferences_get_short(const char* key, int16_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  int16_t value = default_value;
  _return_err = nvs_get_i16(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

uint16_t preferences_get_ushort(const char* key, uint16_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  uint16_t value = default_value;
  _return_err = nvs_get_u16(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

int32_t preferences_get_int(const char* key, int32_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  int32_t value = default_value;
  _return_err = nvs_get_i32(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

uint32_t preferences_get_uint(const char* key, uint32_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  uint32_t value = default_value;
  _return_err = nvs_get_u32(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

int32_t preferences_get_long(const char* key, int32_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  int32_t value = default_value;
  _return_err = nvs_get_i32(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

uint32_t preferences_get_ulong(const char* key, uint32_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  uint32_t value = default_value;
  _return_err = nvs_get_u32(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

int64_t preferences_get_long64(const char* key, int64_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  int64_t value = default_value;
  _return_err = nvs_get_i64(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
    return default_value;
  }

  return value;
}

uint64_t preferences_get_ulong64(const char* key, uint64_t default_value) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return default_value;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return default_value; }

  uint64_t value = default_value;
  _return_err = nvs_get_u64(_nvs_handler, key, &value);
  xSemaphoreGive(_pref_mutex);
  if (_return_err != ESP_OK) {
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
  return preferences_get_uchar(key, default_value ? 1 : 0) == 1;
}

esp_err_t preferences_get_string(const char* key,
                                 char* value,
                                 size_t max_length) {
  if (value == NULL || max_length == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!_check_started()) {
    xSemaphoreGive(_pref_mutex);
    return ESP_ERR_INVALID_STATE;
  }

  size_t length = 0;
  _return_err = nvs_get_str(_nvs_handler, key, NULL, &length);

  if (_return_err != ESP_OK) {
    xSemaphoreGive(_pref_mutex);
    return _return_err;
  }

  if (length > max_length) {
    ESP_LOGE(TAG, "The value is too long for the buffer!");
    xSemaphoreGive(_pref_mutex);
    return ESP_ERR_NVS_INVALID_LENGTH;
  }

  _return_err = nvs_get_str(_nvs_handler, key, value, &length);
  xSemaphoreGive(_pref_mutex);
  return _return_err;
}

size_t preferences_get_bytes_length(const char* key) {
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return 0;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return 0; }

  size_t length = 0;
  _return_err = nvs_get_blob(_nvs_handler, key, NULL, &length);
  xSemaphoreGive(_pref_mutex);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return 0;
  }

  return length;
}

esp_err_t preferences_get_bytes(const char* key, void* buffer, size_t length) {
  if (buffer == NULL || length == 0) return ESP_ERR_INVALID_ARG;
  if (!_ensure_mutex() || xSemaphoreTake(_pref_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_INVALID_STATE;
  if (!_check_started()) { xSemaphoreGive(_pref_mutex); return ESP_ERR_INVALID_STATE; }

  _return_err = nvs_get_blob(_nvs_handler, key, buffer, &length);
  xSemaphoreGive(_pref_mutex);
  if (_return_err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "The value '%s' is not initialized yet!", key);
    return ESP_ERR_NVS_NOT_FOUND;
  }

  return _return_err;
}
