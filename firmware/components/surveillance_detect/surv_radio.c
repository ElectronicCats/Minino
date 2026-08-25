// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_radio.h"
#include <string.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "surv_engine.h"
#include "surv_match.h"
#include "surv_signatures.h"

#define TAG "surv_radio"

#define SURV_DWELL_MS 250

static const uint8_t HOP_PRIMARY[] = {11, 6, 1};
static const uint8_t HOP_EXTENDED[] = {13, 8, 3};

static volatile bool s_running = false;
static surv_profile_t s_profile = SURV_PROFILE_SURVEIL;
static bool s_active_scan = false;
static volatile uint8_t s_current_channel = 1;
static TaskHandle_t s_radio_task = NULL;
// Estado del escaneo BLE: deseo (ventana activa) vs realidad del controlador
// (SCAN_START/STOP_COMPLETE). El auto-restart en INQ_CMPL_EVT solo debe
// ocurrir si de verdad queremos escanear: en SURVEIL la ventana BLE se apaga
// para cederle el aire al sniffer WiFi, y un restart incondicional mantenia
// el escaneo compitiendo con la captura promiscua.
static volatile bool s_ble_scan_desired = false;
static volatile bool s_ble_scanning = false;
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_ble_scan_params_t s_ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x50,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
};

static bool channel_allowed(uint8_t ch) {
  wifi_country_t c;
  if (esp_wifi_get_country(&c) != ESP_OK) {
    return ch <= 11;
  }
  return ch >= c.schan && ch < (uint8_t) (c.schan + c.nchan);
}

uint8_t surv_radio_current_channel(void) {
  return s_current_channel;
}

// Intenta arrancar el escaneo solo si hay deseo activo y el controlador aun
// no esta escaneando. Evita llamar start_scanning() sobre un escaneo ya en
// curso: segun la version de Bluedroid, el segundo start puede DETENER el
// escaneo en vez de ignorarse.
// NOTA: portENTER_CRITICAL/EXIT para eliminar la carrera TOCTOU entre la
// llamada desde ble_gap_cb (tarea Bluedroid) y ble_window_ms (radio_task).
static void ble_try_start_scan(uint32_t seconds) {
  portENTER_CRITICAL(&s_ble_mux);
  if (!s_running || !s_ble_scan_desired || s_ble_scanning ||
      s_profile == SURV_PROFILE_FLOCK) {
    portEXIT_CRITICAL(&s_ble_mux);
    return;
  }
  s_ble_scanning = true;  // pre-set para prevenir doble start
  portEXIT_CRITICAL(&s_ble_mux);
  esp_err_t err = esp_ble_gap_start_scanning(seconds);
  if (err != ESP_OK) {
    portENTER_CRITICAL(&s_ble_mux);
    s_ble_scanning = false;
    portEXIT_CRITICAL(&s_ble_mux);
    ESP_LOGW(TAG, "start_scanning(%lu): %s", (unsigned long) seconds,
             esp_err_to_name(err));
  }
}

static void ble_gap_cb(esp_gap_ble_cb_event_t event,
                       esp_ble_gap_cb_param_t* param) {
  if (param == NULL) {
    return;
  }
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      ble_try_start_scan(30);
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      s_ble_scanning =
          (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS);
      if (!s_ble_scanning) {
        ESP_LOGW(TAG, "BLE scan start fallo: 0x%x",
                 param->scan_start_cmpl.status);
        // Reintento asincrono: reconfigurar params dispara
        // SCAN_PARAM_SET_COMPLETE_EVT y vuelve a intentar arrancar.
        if (s_running && s_ble_scan_desired &&
            s_profile != SURV_PROFILE_FLOCK) {
          esp_ble_gap_set_scan_params(&s_ble_scan_params);
        }
      }
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      s_ble_scanning = false;
      break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      esp_ble_gap_cb_param_t* scan_rst = param;
      switch (scan_rst->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          if (scan_rst->scan_rst.rssi < SURV_RSSI_MIN) {
            break;
          }
          surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
          uint8_t n = 0;
          if (scan_rst->scan_rst.adv_data_len > 0) {
            n = surv_match_ble_adv(scan_rst->scan_rst.ble_adv,
                                   scan_rst->scan_rst.adv_data_len, hits);
          }
          // Checar OUI en direccion BLE (para Axon, etc.)
          const surv_oui_entry_t* e = surv_match_oui(scan_rst->scan_rst.bda);
          if (e != NULL && n < SURV_BLE_MAX_HITS) {
            bool already = false;
            for (uint8_t i = 0; i < n; i++) {
              if (hits[i].klass == e->klass) {
                already = true;
                break;
              }
            }
            if (!already) {
              hits[n].klass = e->klass;
              hits[n].points = e->points;
              hits[n].label = "OUI BLE";
              n++;
            }
          }

          for (uint8_t i = 0; i < n; i++) {
            surv_event_t ev = {0};
            memcpy(ev.mac, scan_rst->scan_rst.bda, 6);
            ev.klass = hits[i].klass;
            // Aplicar techo de tier: los hits de adv usan ADDR2 como tier
            // base, pero si el match viene de OUI, respetar el techo de
            // confianza de la entrada OUI (igual que la ruta WiFi).
            if (e != NULL && hits[i].klass == e->klass) {
              ev.tier = surv_clamp_tier(SURV_TIER_ADDR2, e->tier);
            } else {
              ev.tier = SURV_TIER_ADDR2;
            }
            ev.rssi = scan_rst->scan_rst.rssi;
            ev.channel = 0;
            ev.proto = SURV_PROTO_BLE;
            surv_queue_push(&ev, hits[i].points);
          }
          // Si no hubo hits de adv ni de OUI, registrar MAC desconocida para
          // el rastreador de persistencia (detecta trackers no catalogados).
          if (n == 0) {
            uint32_t now = (uint32_t) (esp_timer_get_time() / 1000);
            surv_engine_note_unknown(scan_rst->scan_rst.bda, now);
          }

          break;
        }
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
          s_ble_scanning = false;
          ble_try_start_scan(30);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
}

static void wifi_window_ms(uint32_t ms) {
  if (!s_running) {
    return;
  }
  esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
  esp_wifi_set_promiscuous(true);

  uint32_t elapsed = 0;
  uint8_t hop_idx = 0;
  uint8_t round = 0;

  while (s_running && elapsed < ms) {
    uint8_t ch =
        HOP_PRIMARY[hop_idx % (sizeof(HOP_PRIMARY) / sizeof(HOP_PRIMARY[0]))];
    if (s_profile == SURV_PROFILE_SURVEIL && (round % 4 == 3)) {
      ch = HOP_EXTENDED[hop_idx %
                        (sizeof(HOP_EXTENDED) / sizeof(HOP_EXTENDED[0]))];
    }
    if (channel_allowed(ch)) {
      s_current_channel = ch;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    }
    hop_idx++;
    if (hop_idx >= 3) {
      hop_idx = 0;
      round++;
    }
    vTaskDelay(pdMS_TO_TICKS(SURV_DWELL_MS));
    elapsed += SURV_DWELL_MS;
  }

  esp_wifi_set_promiscuous(false);
}

static void ble_window_ms(uint32_t ms) {
  if (!s_running || s_profile == SURV_PROFILE_FLOCK) {
    return;
  }
  s_current_channel = 0;
  // Marcar deseo y dejar que los eventos GAP sincronicen el estado real.
  // En TRACKERS el escaneo vive continuo (auto-restart en INQ_CMPL_EVT);
  // en SURVEIL solo durante esta ventana, sin robarle aire al sniffer.
  s_ble_scan_desired = true;
  ble_try_start_scan((ms + 999) / 1000);
  uint32_t elapsed = 0;
  while (s_running && elapsed < ms) {
    vTaskDelay(pdMS_TO_TICKS(100));
    elapsed += 100;
  }
  if (s_profile != SURV_PROFILE_TRACKERS) {
    s_ble_scan_desired = false;
    if (s_ble_scanning) {
      esp_ble_gap_stop_scanning();
    }
  }
}

static void radio_task(void* arg) {
  (void) arg;
  while (s_running) {
    switch (s_profile) {
      case SURV_PROFILE_FLOCK:
        wifi_window_ms(20000);
        break;
      case SURV_PROFILE_SURVEIL:
        ble_window_ms(6000);
        wifi_window_ms(14000);
        break;
      case SURV_PROFILE_TRACKERS:
        ble_window_ms(20000);
        break;
    }
    if (s_active_scan && s_profile != SURV_PROFILE_TRACKERS && s_running) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  s_radio_task = NULL;
  vTaskDelete(NULL);
}

#include "wifi_controller.h"

esp_err_t surv_radio_start(surv_profile_t p, bool active_scan) {
  s_profile = p;
  s_active_scan = active_scan;

  // Inicializar Wi-Fi a traves del controlador estandar del proyecto
  wifi_driver_init_sta();

  // Inicializar BLE. No tragarnos los errores en silencio: sin log, un fallo
  // de init se manifiesta como "no detecta nada" sin pista alguna.
  esp_err_t ret;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_init: %s", esp_err_to_name(ret));
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bt_controller_enable: %s", esp_err_to_name(ret));
    }
  }

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bd_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_init_with_cfg: %s", esp_err_to_name(ret));
    }
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "bluedroid_enable: %s", esp_err_to_name(ret));
    }
  }

  s_running = true;

  ret = esp_ble_gap_register_callback(ble_gap_cb);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap_register_callback: %s", esp_err_to_name(ret));
  }
  // Asincrono: cuando termine, ble_gap_cb recibira SCAN_PARAM_SET_COMPLETE_EVT
  // y arrancara el escaneo si s_running ya es true y hay deseo de escanear.
  if (p != SURV_PROFILE_FLOCK) {
    s_ble_scan_desired = true;
    s_ble_scan_params.scan_type = active_scan ? BLE_SCAN_TYPE_ACTIVE
                                              : BLE_SCAN_TYPE_PASSIVE;
    ret = esp_ble_gap_set_scan_params(&s_ble_scan_params);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "set_scan_params: %s", esp_err_to_name(ret));
    }
  }

  if (xTaskCreate(radio_task, "surv_radio", 4096, NULL, 5, &s_radio_task) !=
      pdPASS) {
    s_running = false;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void surv_radio_stop(void) {
  s_ble_scan_desired = false;
  s_running = false;
  esp_wifi_set_promiscuous(false);
  esp_ble_gap_stop_scanning();
  // Esperar a que la tarea radio_task termine y se auto-elimine. Sin esto,
  // surv_radio_start() podria ejecutarse antes de que la tarea anterior haya
  // liberado el CPU, provocando condiciones de carrera en los recursos de
  // radio (WiFi promiscuo, BLE GAP).
  while (s_radio_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
