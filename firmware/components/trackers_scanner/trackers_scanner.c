
#include "trackers_scanner.h"
#include <math.h>
#include <string.h>
#include "bt_gattc.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "surv_match.h"

static TaskHandle_t trackers_scan_timer_task = NULL;
static bluetooth_traker_scanner_cb_t display_records_cb = NULL;
static int trackers_scan_duration = 0;
static bool trackers_scanner_active = false;
// Guarda de reentrada para trackers_scanner_stop(). El teardown de BT es
// bloqueante y corre tanto desde el handler de boton como desde
// task_tracker_timer (auto-stop por duracion); si ambos entran a la vez,
// cada uno esperaria el semaforo de esp_bluedroid_disable()/controller que el
// otro consume, y la UI se congela para siempre.
static bool trackers_stop_in_progress = false;
static portMUX_TYPE trackers_stop_mux = portMUX_INITIALIZER_UNLOCKED;

static void task_tracker_timer();
static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record);
static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param);

void trackers_scanner_start() {
  bt_gattc_set_passive_scan(true);
  gattc_scan_params_t scan_params = {
      .remote_filter_service_uuid =
          bt_gattc_set_default_ble_filter_service_uuid(),
      .remote_filter_char_uuid = bt_gattc_set_default_ble_filter_char_uuid(),
      .notify_descr_uuid = bt_gattc_set_default_ble_notify_descr_uuid(),
      .ble_scan_params = bt_gattc_set_default_ble_scan_params()};
  bt_gattc_set_ble_scan_params(&scan_params);
  bt_client_event_cb_t event_cb = {.handler_gattc_cb = NULL,
                                   .handler_gapc_cb = handle_bt_gapc_events};
  bt_gattc_set_cb(event_cb);
  bt_gattc_task_begin();
  trackers_scanner_active = true;
  xTaskCreate(task_tracker_timer, "Trackers_task", 4096, NULL, 5,
              &trackers_scan_timer_task);
}

static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param) {
  switch (event_type) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*) param;
      switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
          if (!trackers_scanner_active) {
            break;
          }
          tracker_profile_t tracker_record = {
              .rssi = 0,
              .name = "",
              .vendor = "",
              .mac_address = {0},
              .adv_data = {0},
              .adv_data_length = 0,
              .is_tracker = false,
          };
          if (scan_result->scan_rst.adv_data_len > 0) {
            tracker_dissector(scan_result, &tracker_record);

            if (tracker_record.is_tracker) {
              if (display_records_cb) {
                display_records_cb(tracker_record);
              }
            }
          }
          break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void trackers_scanner_register_cb(bluetooth_traker_scanner_cb_t callback) {
  display_records_cb = callback;
}

static void task_tracker_timer() {
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task started");
  trackers_scan_duration = 0;
  while (trackers_scanner_active) {
    trackers_scan_duration++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  trackers_scan_timer_task = NULL;
  vTaskDelete(NULL);
}

void trackers_scanner_stop() {
  portENTER_CRITICAL(&trackers_stop_mux);
  if (trackers_stop_in_progress || !trackers_scanner_active) {
    portEXIT_CRITICAL(&trackers_stop_mux);
    return;
  }
  trackers_stop_in_progress = true;
  portEXIT_CRITICAL(&trackers_stop_mux);

  trackers_scanner_active = false;
  trackers_scan_duration = 0;
  bt_gattc_task_stop();
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
  TaskHandle_t task_to_delete = trackers_scan_timer_task;
  trackers_scan_timer_task = NULL;
  if (task_to_delete != NULL && task_to_delete != xTaskGetCurrentTaskHandle()) {
    vTaskDelete(task_to_delete);
  }

  portENTER_CRITICAL(&trackers_stop_mux);
  trackers_stop_in_progress = false;
  portEXIT_CRITICAL(&trackers_stop_mux);
}

static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record) {
  uint8_t* adv = scan_rst->scan_rst.ble_adv;
  uint8_t adv_len = scan_rst->scan_rst.adv_data_len;

  tracker_record->is_tracker = false;

  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, adv_len, hits);

  // Esta app solo muestra rastreadores personales. El resto de clases que el
  // disector compartido reconoce (Flock, Axon, drones) las consume la app de
  // vigilancia, no esta.
  // El dissector compartido solo da la etiqueta de clase (p.ej. "AirTag");
  // el fabricante no vive en ella. Se mapea por clase para que la UI muestre
  // name="AirTag" vendor="Apple" en vez de la misma etiqueta dos veces, como
  // hacia el disector local que esto reemplaza.
  for (uint8_t i = 0; i < n; i++) {
    switch (hits[i].klass) {
      case SURV_CLASS_AIRTAG:
      case SURV_CLASS_APPLE_NEARBY:
        tracker_record->is_tracker = true;
        tracker_record->name = (char*) hits[i].label;
        tracker_record->vendor = "Apple";
        break;
      case SURV_CLASS_SMARTTAG:
        tracker_record->is_tracker = true;
        tracker_record->name = (char*) hits[i].label;
        tracker_record->vendor = "Samsung";
        break;
      case SURV_CLASS_TILE:
        tracker_record->is_tracker = true;
        tracker_record->name = (char*) hits[i].label;
        tracker_record->vendor = "Tile";
        break;
      default:
        break;
    }
  }

  if (tracker_record->is_tracker) {
    tracker_record->rssi = scan_rst->scan_rst.rssi;
    tracker_record->adv_data_length = adv_len;
    memcpy(tracker_record->mac_address, scan_rst->scan_rst.bda, 6);
    size_t copy_len = (adv_len > sizeof(tracker_record->adv_data))
                          ? sizeof(tracker_record->adv_data)
                          : adv_len;
    memcpy(tracker_record->adv_data, adv, copy_len);
  }
}

#define TRACKERS_MAX_PROFILES 50

void trackers_scanner_add_tracker_profile(tracker_profile_t** profiles,
                                          uint16_t* num_profiles,
                                          tracker_profile_t new_profile) {
  if (profiles == NULL || num_profiles == NULL) {
    return;
  }
  if (*num_profiles >= TRACKERS_MAX_PROFILES) {
    ESP_LOGW(TAG_BLE_CLIENT_MODULE, "Maximum tracker profiles reached (%d)",
             TRACKERS_MAX_PROFILES);
    return;
  }
  tracker_profile_t* temp =
      realloc(*profiles, (*num_profiles + 1) * sizeof(tracker_profile_t));
  if (temp == NULL) {
    ESP_LOGE(TAG_BLE_CLIENT_MODULE, "Failed to reallocate tracker profiles");
    return;
  }
  *profiles = temp;
  (*profiles)[*num_profiles] = new_profile;
  (*num_profiles)++;
}

int trackers_scanner_find_profile_by_mac(tracker_profile_t* profiles,
                                         uint16_t num_profiles,
                                         uint8_t mac_address[6]) {
  for (int i = 0; i < num_profiles; i++) {
    if (memcmp(profiles[i].mac_address, mac_address, 6) == 0) {
      return i;  // Profile found
    }
  }
  return -1;  // Profile not found
}

bool trackers_scanner_is_active() {
  return trackers_scanner_active;
}

float trackers_scanner_rssi_to_distance(int rssi) {
  // Log-distance path loss model: d = 10^((TxPower - RSSI) / (10 * n))
  // TxPower = RSSI at 1 meter (calibrated for typical BLE trackers)
  // n = path loss exponent (2.0 = free space, 2.5-3.5 = indoor/obstructed)
  const float tx_power = -59.0f;
  const float path_loss_exponent = 2.5f;
  if (rssi == 0) {
    return -1.0f;
  }
  float ratio = (tx_power - (float) rssi) / (10.0f * path_loss_exponent);
  float distance = powf(10.0f, ratio);
  if (distance < 0.1f) {
    distance = 0.1f;
  }
  if (distance > 100.0f) {
    distance = 100.0f;
  }
  return distance;
}
