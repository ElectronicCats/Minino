/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "radio_lease.h"

#include "sdkconfig.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "radio_lease_registry.h"
#include "wifi_controller.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "gatt_dispatcher.h"
#include "gap_dispatcher.h"
#endif

static const char* TAG = "radio_lease";

/* ------------------------------------------------------------------ */
/* Promiscuous mode                                                    */
/* ------------------------------------------------------------------ */

static radio_lease_registry_t s_prom = {0};

/* The two stack refcounts and their lock are also consulted by the
   promiscuous entry points above the WiFi section, so they live here. */
static uint8_t s_wifi_refs = 0;
static uint8_t s_ble_refs = 0;
static portMUX_TYPE s_radio_mux = portMUX_INITIALIZER_UNLOCKED;

/* Bridges the registry's generic receiver signature to IDF's. */
static void prom_dispatch(void* ctx, const void* buf, int type) {
  wifi_promiscuous_cb_t cb = (wifi_promiscuous_cb_t) ctx;
  if (cb != NULL) {
    cb((void*) buf, (wifi_promiscuous_pkt_type_t) type);
  }
}

/* The single callback IDF keeps. Matches wifi_promiscuous_cb_t exactly;
   it must not be a cast of radio_lease_registry_dispatch(), whose first
   argument is the registry rather than the frame. */
static void prom_driver_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  radio_lease_registry_dispatch(&s_prom, buf, (int) type);
}

const char* radio_lease_user_name(radio_prom_user_t user) {
  switch (user) {
    case RADIO_PROM_SURVEILLANCE:
      return "surveillance";
    case RADIO_PROM_SNIFFER:
      return "sniffer";
    case RADIO_PROM_DEAUTH_DETECTOR:
      return "deauth-detector";
    case RADIO_PROM_DRONEID_SCANNER:
      return "droneid";
    case RADIO_PROM_SPECTRUM:
      return "spectrum";
    case RADIO_PROM_SDR_RX:
      return "sdr-rx";
    case RADIO_PROM_NONE:
    default:
      return "none";
  }
}

esp_err_t radio_lease_prom_acquire(radio_prom_user_t user,
                                   wifi_promiscuous_cb_t cb) {
  if (user <= RADIO_PROM_NONE || user >= RADIO_PROM_USER_COUNT || cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  portENTER_CRITICAL(&s_radio_mux);
  if (!radio_lease_stack_allowed(s_wifi_refs, s_ble_refs,
                                 RADIO_STACK_WIFI)) {
    portEXIT_CRITICAL(&s_radio_mux);
    ESP_LOGW(TAG, "promiscuous requested while BLE is active");
    return ESP_ERR_NOT_SUPPORTED;
  }
  uint8_t before = radio_lease_registry_count(&s_prom);
  esp_err_t err = radio_lease_registry_acquire(&s_prom, (uint8_t) user,
                                               prom_dispatch, (void*) cb);
  portEXIT_CRITICAL(&s_radio_mux);
  if (err != ESP_OK) {
    return err;
  }

  if (before == 0) {
    /* First consumer: this is the only place that turns the driver on. */
    esp_wifi_set_promiscuous_rx_cb(prom_driver_cb);
    esp_err_t perr = esp_wifi_set_promiscuous(true);
    if (perr != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_promiscuous(true): %s",
               esp_err_to_name(perr));
      radio_lease_registry_release(&s_prom, (uint8_t) user);
      return perr;
    }
  }
  ESP_LOGI(TAG, "%s joined promiscuous mode (%u consumer(s))",
           radio_lease_user_name(user), radio_lease_registry_count(&s_prom));
  return ESP_OK;
}

/* Stands in for a receiver: the registry needs a non-NULL entry, but this
   consumer genuinely does not want the frames. */
static void prom_ignore(void* ctx, const void* buf, int type) {
  (void) ctx;
  (void) buf;
  (void) type;
}

esp_err_t radio_lease_prom_hold(radio_prom_user_t user) {
  if (user <= RADIO_PROM_NONE || user >= RADIO_PROM_USER_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  portENTER_CRITICAL(&s_radio_mux);
  if (!radio_lease_stack_allowed(s_wifi_refs, s_ble_refs,
                                 RADIO_STACK_WIFI)) {
    portEXIT_CRITICAL(&s_radio_mux);
    ESP_LOGW(TAG, "promiscuous requested while BLE is active");
    return ESP_ERR_NOT_SUPPORTED;
  }
  uint8_t before = radio_lease_registry_count(&s_prom);
  esp_err_t err = radio_lease_registry_acquire(&s_prom, (uint8_t) user,
                                               prom_ignore, NULL);
  portEXIT_CRITICAL(&s_radio_mux);
  if (err != ESP_OK) {
    return err;
  }
  if (before == 0) {
    esp_wifi_set_promiscuous_rx_cb(prom_driver_cb);
    esp_err_t perr = esp_wifi_set_promiscuous(true);
    if (perr != ESP_OK) {
      radio_lease_registry_release(&s_prom, (uint8_t) user);
      return perr;
    }
  }
  return ESP_OK;
}

esp_err_t radio_lease_prom_set_cb(radio_prom_user_t user,
                                  wifi_promiscuous_cb_t cb) {
  if (user <= RADIO_PROM_NONE || user >= RADIO_PROM_USER_COUNT || cb == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  return radio_lease_registry_set_cb(&s_prom, (uint8_t) user, prom_dispatch,
                                     (void*) cb);
}

esp_err_t radio_lease_prom_release(radio_prom_user_t user) {
  if (user <= RADIO_PROM_NONE || user >= RADIO_PROM_USER_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = radio_lease_registry_release(&s_prom, (uint8_t) user);
  if (err != ESP_OK) {
    return err;
  }

  if (radio_lease_registry_count(&s_prom) == 0) {
    /* Last consumer out: only now is it safe to take the driver down. */
    esp_wifi_set_promiscuous(false);
  }
  return ESP_OK;
}

uint8_t radio_lease_prom_count(void) {
  return radio_lease_registry_count(&s_prom);
}

bool radio_lease_prom_held(radio_prom_user_t user) {
  return radio_lease_registry_holds(&s_prom, (uint8_t) user);
}

esp_err_t radio_lease_prom_set_filter(const wifi_promiscuous_filter_t* filter) {
  if (filter == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  return esp_wifi_set_promiscuous_filter(filter);
}

/* ------------------------------------------------------------------ */
/* WiFi driver                                                         */
/* ------------------------------------------------------------------ */

static radio_wifi_init_mode_t s_wifi_mode = RADIO_WIFI_MODE_STA;
static uint8_t s_ble_scan_block = 0;

static esp_err_t wifi_init_for_mode(radio_wifi_init_mode_t mode) {
  bool ok;
  switch (mode) {
    case RADIO_WIFI_MODE_NULL:
      ok = wifi_driver_init_null();
      break;
    case RADIO_WIFI_MODE_APSTA:
      ok = wifi_driver_init_apsta();
      break;
    case RADIO_WIFI_MODE_STA:
    default:
      ok = wifi_driver_init_sta();
      break;
  }
  if (!ok) {
    return ESP_ERR_INVALID_STATE;
  }
  s_wifi_mode = mode;
  return ESP_OK;
}

esp_err_t radio_lease_wifi_acquire(radio_wifi_init_mode_t mode) {
  portENTER_CRITICAL(&s_radio_mux);
  if (!radio_lease_stack_allowed(s_wifi_refs, s_ble_refs, RADIO_STACK_WIFI)) {
    portEXIT_CRITICAL(&s_radio_mux);
    /* No coexistence: asking for the WiFi driver while BLE is up used to
       deinitialise the driver under the other holder. Reject instead. */
    ESP_LOGW(TAG, "WiFi requested while BLE is active");
    return ESP_ERR_NOT_SUPPORTED;
  }
  bool first = (s_wifi_refs == 0);
  if (s_wifi_refs < 0xFF) {
    s_wifi_refs++;
  }
  portEXIT_CRITICAL(&s_radio_mux);

  if (!first) {
    if (mode != s_wifi_mode) {
      /* Not fatal: the driver is shared and the first holder set it up. */
      ESP_LOGW(TAG, "WiFi already up in mode %d; keeping it (asked for %d)",
               (int) s_wifi_mode, (int) mode);
    }
    return ESP_OK;
  }

  esp_err_t err = wifi_init_for_mode(mode);
  if (err != ESP_OK) {
    portENTER_CRITICAL(&s_radio_mux);
    if (s_wifi_refs > 0) {
      s_wifi_refs--;
    }
    portEXIT_CRITICAL(&s_radio_mux);
    ESP_LOGE(TAG, "WiFi bring-up failed: %s", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

void radio_lease_wifi_release(void) {
  portENTER_CRITICAL(&s_radio_mux);
  if (s_wifi_refs == 0) {
    portEXIT_CRITICAL(&s_radio_mux);
    ESP_LOGW(TAG, "radio_lease_wifi_release() without a matching acquire");
    return;
  }
  s_wifi_refs--;
  bool last = (s_wifi_refs == 0) && radio_lease_registry_count(&s_prom) == 0;
  portEXIT_CRITICAL(&s_radio_mux);
  if (!last) {
    return;
  }
  /* Last holder and no promiscuous consumer: safe to take the driver down. */
  s_wifi_mode = RADIO_WIFI_MODE_STA;
  wifi_driver_deinit_if_started();
}

bool radio_lease_wifi_shared(void) {
  return s_wifi_refs > 0;
}

uint8_t radio_lease_wifi_count(void) {
  return s_wifi_refs;
}

/* ------------------------------------------------------------------ */
/* BLE stack                                                           */
/* ------------------------------------------------------------------ */

#if CONFIG_BT_ENABLED

/* The refcount is touched from the button state machine, the menus'
 * on-exit callbacks, console commands and the BTC task, so every
 * read-modify-write pair needs a critical section. Without it two racing
 * stop/start paths could both see `before != 0` and skip the bring-up, or
 * both drive the count to zero and tear the stack down under each other. */

static void ble_ref_rollback(void) {
  portENTER_CRITICAL(&s_radio_mux);
  if (s_ble_refs > 0) {
    s_ble_refs--;
  }
  portEXIT_CRITICAL(&s_radio_mux);
}

bool radio_lease_wifi_allowed(void) {
  portENTER_CRITICAL(&s_radio_mux);
  bool allowed = s_ble_refs == 0;
  portEXIT_CRITICAL(&s_radio_mux);
  return allowed;
}

esp_err_t radio_lease_ble_acquire(void) {
  portENTER_CRITICAL(&s_radio_mux);
  bool prom_active = radio_lease_registry_count(&s_prom) > 0;
  if (!radio_lease_stack_allowed(s_wifi_refs, s_ble_refs, RADIO_STACK_BLE) ||
      prom_active) {
    uint8_t wifi_refs = s_wifi_refs;
    portEXIT_CRITICAL(&s_radio_mux);
    /* No coexistence: never take the WiFi driver down under a live holder. */
    ESP_LOGW(TAG, "BLE requested while WiFi has %u holder(s)", wifi_refs);
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (s_ble_refs > 0) {
    if (s_ble_refs < 0xFF) {
      s_ble_refs++;
    }
    portEXIT_CRITICAL(&s_radio_mux);
    return ESP_OK;
  }
  s_ble_refs = 1;
  portEXIT_CRITICAL(&s_radio_mux);

  /* No WiFi holder: the driver is idle and its buffers (~65 KB) can go back
   * to the heap before Bluedroid comes up. */
  wifi_driver_deinit_if_started();

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_init: %s", esp_err_to_name(err));
      ble_ref_rollback();
      return err;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_err_t err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_enable: %s", esp_err_to_name(err));
      ble_ref_rollback();
      return err;
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bluedroid_init_with_cfg(&bd_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_init_with_cfg: %s", esp_err_to_name(err));
      ble_ref_rollback();
      return err;
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    esp_err_t err = esp_bluedroid_enable();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_enable: %s", esp_err_to_name(err));
      ble_ref_rollback();
      return err;
    }
  }
  gap_dispatcher_init();
  return ESP_OK;
}

void radio_lease_ble_release(void) {
  uint8_t after;
  portENTER_CRITICAL(&s_radio_mux);
  if (s_ble_refs == 0) {
    portEXIT_CRITICAL(&s_radio_mux);
    /* Nobody holds a claim: a stray/double release must not tear down a stack
       that may be in use by code that brought it up outside the lease. */
    ESP_LOGW(TAG, "radio_lease_ble_release() without a matching acquire");
    return;
  }
  s_ble_refs--;
  after = s_ble_refs;
  portEXIT_CRITICAL(&s_radio_mux);
  if (after > 0) {
    return;
  }
  gap_dispatcher_reset();
  gatt_dispatcher_reset();
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    esp_bluedroid_disable();
    int timeout = 50; /* 50 x 10ms = 500ms max */
    while (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED && timeout-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    esp_bluedroid_deinit();
    int timeout = 50;
    while (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED && timeout-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
    int timeout = 50;
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED && timeout-- > 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_bt_controller_deinit();
  }
}

bool radio_lease_ble_shared(void) {
  return s_ble_refs > 0;
}

uint8_t radio_lease_ble_count(void) {
  return s_ble_refs;
}

void radio_lease_ble_block_scan(void) {
  portENTER_CRITICAL(&s_radio_mux);
  if (s_ble_scan_block < 0xFF) {
    s_ble_scan_block++;
  }
  portEXIT_CRITICAL(&s_radio_mux);
}

void radio_lease_ble_unblock_scan(void) {
  portENTER_CRITICAL(&s_radio_mux);
  if (s_ble_scan_block > 0) {
    s_ble_scan_block--;
  }
  portEXIT_CRITICAL(&s_radio_mux);
}

bool radio_lease_ble_scan_allowed(void) {
  portENTER_CRITICAL(&s_radio_mux);
  bool allowed = s_ble_scan_block == 0;
  portEXIT_CRITICAL(&s_radio_mux);
  return allowed;
}

#endif


esp_err_t radio_lease_wifi_acquire_timeout(radio_wifi_init_mode_t mode, TickType_t timeout_ticks) {
  TickType_t start = xTaskGetTickCount();
  while (1) {
    esp_err_t err = radio_lease_wifi_acquire(mode);
    if (err == ESP_OK) {
      return ESP_OK;
    }
    if (err != ESP_ERR_NOT_SUPPORTED) {
      return err;
    }
    if (timeout_ticks != portMAX_DELAY) {
      TickType_t elapsed = xTaskGetTickCount() - start;
      if (elapsed >= timeout_ticks) {
        return ESP_ERR_TIMEOUT;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

esp_err_t radio_lease_ble_acquire_timeout(TickType_t timeout_ticks) {
  TickType_t start = xTaskGetTickCount();
  while (1) {
    esp_err_t err = radio_lease_ble_acquire();
    if (err == ESP_OK) {
      return ESP_OK;
    }
    if (err != ESP_ERR_NOT_SUPPORTED) {
      return err;
    }
    if (timeout_ticks != portMAX_DELAY) {
      TickType_t elapsed = xTaskGetTickCount() - start;
      if (elapsed >= timeout_ticks) {
        return ESP_ERR_TIMEOUT;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

