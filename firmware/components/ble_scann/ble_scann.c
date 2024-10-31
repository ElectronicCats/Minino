#include "ble_scann.h"
#include "bt_gattc.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "inttypes.h"
#include "uart_sender.h"

static TaskHandle_t ble_scan_timer_task = NULL;
static bluetooth_adv_scanner_cb_t display_records_cb = NULL;
static int ble_scan_duration = 0;
static bool ble_scanner_active = false;
static esp_ble_scan_filter_t ble_scan_filter = BLE_SCAN_FILTER_ALLOW_ALL;
static esp_ble_scan_type_t ble_scan_type = BLE_SCAN_TYPE_ACTIVE;
static void task_scanner_timer();
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
  xTaskCreate(task_scanner_timer, "ble_scanner", 4096, NULL, 5,
              &ble_scan_timer_task);
}

static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param) {
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

void ble_scanner_register_cb(bluetooth_adv_scanner_cb_t callback) {
  display_records_cb = callback;
}

static void task_scanner_timer() {
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task started");
  ble_scan_duration = 0;
  while (ble_scanner_active) {
    if (ble_scan_duration >= SCANNER_SCAN_DURATION) {
      ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
      ble_scanner_stop();
    }
    ble_scan_duration++;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ble_scanner_stop() {
  ble_scanner_active = false;
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
  if (ble_scan_timer_task != NULL) {
    ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
    vTaskSuspend(ble_scan_timer_task);
  }
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
  ble_scan_duration = 0;
  vTaskDelete(NULL);
  // TODO: When this is called, the BLE stopping bricks the device
  // bt_gattc_task_stop();
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
}

bool ble_scanner_is_active() {
  return ble_scanner_active;
}
