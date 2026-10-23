#include "sdkconfig.h"

#if CONFIG_BT_ENABLED
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "gatt_dispatcher.h"

#include <stdbool.h>
#include <string.h>

#include "esp_bt_main.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define GATT_SUBSCRIBER_MAX 8
#define TAG                 "gatt_dispatcher"

typedef struct {
  esp_gattc_cb_t cb;
  bool used;
} gatt_subscriber_t;

static gatt_subscriber_t s_subscribers[GATT_SUBSCRIBER_MAX];
/* True while esp_ble_gattc_register_callback(gatt_dispatcher_dispatch) has
 * been accepted by the currently enabled Bluedroid. Reset whenever the stack
 * is torn down (radio_lease_release -> gatt_dispatcher_reset). */
static bool s_installed = false;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static void gatt_dispatcher_dispatch(esp_gattc_cb_event_t event,
                                     esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t* param) {
  esp_gattc_cb_t local_cbs[GATT_SUBSCRIBER_MAX];
  uint8_t count = 0;

  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < GATT_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb != NULL) {
      local_cbs[count++] = s_subscribers[i].cb;
    }
  }
  portEXIT_CRITICAL(&s_mux);

  for (uint8_t i = 0; i < count; i++) {
    local_cbs[i](event, gattc_if, param);
  }
}

esp_err_t gatt_dispatcher_init(void) {
  portENTER_CRITICAL(&s_mux);
  if (!s_installed) {
    memset(s_subscribers, 0, sizeof(s_subscribers));
  }
  portEXIT_CRITICAL(&s_mux);

  esp_bluedroid_status_t status = esp_bluedroid_get_status();
  if (status != ESP_BLUEDROID_STATUS_ENABLED) {
    /* Nothing to register into yet; the next register() call after the stack
     * comes back up will install the dispatch callback. */
    return ESP_OK;
  }

  bool need_install = false;
  portENTER_CRITICAL(&s_mux);
  if (!s_installed) {
    need_install = true;
  }
  portEXIT_CRITICAL(&s_mux);
  if (!need_install) {
    return ESP_OK;
  }

  esp_err_t ret = esp_ble_gattc_register_callback(gatt_dispatcher_dispatch);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "gattc_register_callback failed: %s", esp_err_to_name(ret));
    return ret;
  }

  portENTER_CRITICAL(&s_mux);
  s_installed = true;
  portEXIT_CRITICAL(&s_mux);
  return ESP_OK;
}

esp_err_t gatt_dispatcher_register(esp_gattc_cb_t cb) {
  if (cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t init_ret = gatt_dispatcher_init();
  if (init_ret != ESP_OK) {
    return init_ret;
  }

  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < GATT_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb == cb) {
      portEXIT_CRITICAL(&s_mux);
      return ESP_OK;  // Already registered, nothing to do.
    }
  }
  for (int i = 0; i < GATT_SUBSCRIBER_MAX; i++) {
    if (!s_subscribers[i].used) {
      s_subscribers[i].cb = cb;
      s_subscribers[i].used = true;
      portEXIT_CRITICAL(&s_mux);
      return ESP_OK;
    }
  }
  portEXIT_CRITICAL(&s_mux);
  ESP_LOGE(TAG, "subscriber list full");
  return ESP_ERR_NO_MEM;
}

esp_err_t gatt_dispatcher_unregister(esp_gattc_cb_t cb) {
  if (cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < GATT_SUBSCRIBER_MAX; i++) {
    if (s_subscribers[i].used && s_subscribers[i].cb == cb) {
      s_subscribers[i].cb = NULL;
      s_subscribers[i].used = false;
      portEXIT_CRITICAL(&s_mux);
      return ESP_OK;
    }
  }
  portEXIT_CRITICAL(&s_mux);
  return ESP_ERR_NOT_FOUND;
}

esp_err_t gatt_dispatcher_reset(void) {
  portENTER_CRITICAL(&s_mux);
  s_installed = false;
  portEXIT_CRITICAL(&s_mux);
  return ESP_OK;
}

#endif