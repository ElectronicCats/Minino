#include "sdkconfig.h"

#if CONFIG_BT_ENABLED

  #include "gap_dispatcher.h"

  #include <stdbool.h>
  #include <string.h>

  #include "esp_bt_main.h"
  #include "esp_log.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"

  #define GAP_SUBSCRIBER_MAX 16
  #define TAG                "gap_dispatcher"

typedef struct {
  esp_gap_ble_cb_t cb;
  bool used;
} gap_subscriber_t;

static gap_subscriber_t s_subscribers[GAP_SUBSCRIBER_MAX];
static bool s_container_initialized = false;
/* Bluedroid status observed on the previous gap_dispatcher_init() call. The
   dispatcher installs itself into Bluedroid on every *transition* into
   ENABLED, because esp_ble_gap_register_callback() overwrites whatever the
   stack had, and a disable/enable cycle (surv_radio does this) wipes it. */
static esp_bluedroid_status_t s_last_seen_status =
    ESP_BLUEDROID_STATUS_UNINITIALIZED;
static portMUX_TYPE s_dispatcher_mux = portMUX_INITIALIZER_UNLOCKED;

static void gap_dispatcher_dispatch(esp_gap_ble_cb_event_t event,
                                    esp_ble_gap_cb_param_t* param) {
  esp_gap_ble_cb_t local_cbs[GAP_SUBSCRIBER_MAX];
  uint8_t count = 0;

  portENTER_CRITICAL(&s_dispatcher_mux);
  for (int i = 0; i < GAP_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb != NULL) {
      local_cbs[count++] = s_subscribers[i].cb;
    }
  }
  portEXIT_CRITICAL(&s_dispatcher_mux);

  for (uint8_t i = 0; i < count; i++) {
    local_cbs[i](event, param);
  }
}

esp_err_t gap_dispatcher_init(void) {
  portENTER_CRITICAL(&s_dispatcher_mux);
  if (!s_container_initialized) {
    memset(s_subscribers, 0, sizeof(s_subscribers));
    s_container_initialized = true;
  }
  portEXIT_CRITICAL(&s_dispatcher_mux);

  // esp_ble_gap_register_callback() overwrites the stack's previous handler, so
  // it has to run again on every transition into ENABLED. While Bluedroid just
  // stays ENABLED the call is skipped, which keeps each subscriber
  // registration from costing a BTC round trip.
  esp_bluedroid_status_t status = esp_bluedroid_get_status();

  portENTER_CRITICAL(&s_dispatcher_mux);
  bool just_enabled = (status == ESP_BLUEDROID_STATUS_ENABLED) &&
                      (s_last_seen_status != ESP_BLUEDROID_STATUS_ENABLED);
  s_last_seen_status = status;
  portEXIT_CRITICAL(&s_dispatcher_mux);

  if (!just_enabled) {
    return ESP_OK;
  }

  esp_err_t ret = esp_ble_gap_register_callback(gap_dispatcher_dispatch);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "gap_register_callback failed: %s", esp_err_to_name(ret));
    portENTER_CRITICAL(&s_dispatcher_mux);
    s_last_seen_status = ESP_BLUEDROID_STATUS_UNINITIALIZED;
    portEXIT_CRITICAL(&s_dispatcher_mux);
    return ret;
  }
  return ESP_OK;
}

esp_err_t gap_dispatcher_reset(void) {
  portENTER_CRITICAL(&s_dispatcher_mux);
  s_last_seen_status = ESP_BLUEDROID_STATUS_UNINITIALIZED;
  portEXIT_CRITICAL(&s_dispatcher_mux);
  return ESP_OK;
}

esp_err_t gap_dispatcher_register(esp_gap_ble_cb_t cb) {
  if (cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t init_ret = gap_dispatcher_init();
  if (init_ret != ESP_OK) {
    return init_ret;
  }

  portENTER_CRITICAL(&s_dispatcher_mux);
  for (int i = 0; i < GAP_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb == cb) {
      portEXIT_CRITICAL(&s_dispatcher_mux);
      return ESP_OK;  // Already registered, nothing to do.
    }
  }
  for (int i = 0; i < GAP_SUBSCRIBER_MAX; i++) {
    if (!s_subscribers[i].used) {
      s_subscribers[i].cb = cb;
      s_subscribers[i].used = true;
      portEXIT_CRITICAL(&s_dispatcher_mux);
      return ESP_OK;
    }
  }
  portEXIT_CRITICAL(&s_dispatcher_mux);
  ESP_LOGE(TAG, "subscriber list full");
  return ESP_ERR_NO_MEM;
}

esp_err_t gap_dispatcher_unregister(esp_gap_ble_cb_t cb) {
  if (cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  portENTER_CRITICAL(&s_dispatcher_mux);
  for (int i = 0; i < GAP_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb == cb) {
      s_subscribers[i].cb = NULL;
      s_subscribers[i].used = false;
      portEXIT_CRITICAL(&s_dispatcher_mux);
      return ESP_OK;
    }
  }
  portEXIT_CRITICAL(&s_dispatcher_mux);
  return ESP_ERR_NOT_FOUND;
}

#endif