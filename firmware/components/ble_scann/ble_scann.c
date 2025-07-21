#include "ble_scann.h"
#include "bt_gattc.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "inttypes.h"
#include "uart_sender.h"

static bluetooth_adv_scanner_cb_t display_records_cb = NULL;
static bool ble_scanner_active = false;
static esp_ble_scan_filter_t ble_scan_filter =
    BLE_SCAN_FILTER_ALLOW_UND_RPA_DIR;
static esp_ble_scan_type_t ble_scan_type = BLE_SCAN_TYPE_ACTIVE;
static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param);

void set_filter_type(uint8_t filter_type) {
  ble_scan_filter = filter_type;
}

void set_scan_type(uint8_t scan_type) {
  ble_scan_type = scan_type;
}

void ble_scanner_begin() {
  // #if !defined(CONFIG_TRACKERS_SCANNER_DEBUG)
  //   esp_log_level_set(TAG_BLE_CLIENT_MODULE, ESP_LOG_NONE);
  // #endif

  gattc_scan_params_t scan_params = {
      .remote_filter_service_uuid =
          bt_gattc_set_default_ble_filter_service_uuid(),
      .remote_filter_char_uuid = bt_gattc_set_default_ble_filter_char_uuid(),
      .notify_descr_uuid = bt_gattc_set_default_ble_notify_descr_uuid(),
      .ble_scan_params = bt_gattc_set_default_ble_scan_params()};
  scan_params.ble_scan_params.scan_filter_policy = ble_scan_filter;
  scan_params.ble_scan_params.scan_type = ble_scan_type;
  bt_gattc_set_ble_scan_params(&scan_params);
  bt_client_event_cb_t event_cb = {.handler_gattc_cb = NULL,
                                   .handler_gapc_cb = handle_bt_gapc_events};
  bt_gattc_set_cb(event_cb);
  bt_gattc_task_begin();
  ble_scanner_active = true;
}

static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param) {
  ESP_LOGE("HERE", "event: %d", event_type);
  switch (event_type) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*) param;
      switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
          if (!ble_scanner_active) {
            break;
          }
          uart_sender_send_packet_ble(UART_SENDER_PACKET_TYPE_BLE, scan_result);
          if (display_records_cb != NULL) {
            display_records_cb(scan_result);
          }
          ESP_LOGI(TAG_BLE_CLIENT_MODULE, "New ADV found");
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

void ble_scanner_register_cb(bluetooth_adv_scanner_cb_t callback) {
  display_records_cb = callback;
}

void ble_scanner_stop() {
  ble_scanner_active = false;
  vTaskDelete(NULL);
  // TODO: When this is called, the BLE stopping bricks the device
  // bt_gattc_task_stop();
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
}

bool ble_scanner_is_active() {
  return ble_scanner_active;
}
