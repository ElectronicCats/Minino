
#include "trackers_scanner.h"
#include "bt_gattc.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "inttypes.h"

static TaskHandle_t trackers_scan_timer_task = NULL;
static bluetooth_traker_scanner_cb_t display_records_cb = NULL;
static int trackers_scan_duration = 0;
static bool trackers_scanner_active = false;

// https://adamcatley.com/AirTag
static tracker_adv_cmp_t trackers[] = {
    {.name = "ATag", .vendor = "Apple", .adv_cmp = {0x1E, 0xFF, 0x4C, 0x00}},
    {.name = "UATag", .vendor = "Apple", .adv_cmp = {0x4C, 0x00, 0x12, 0x19}},
    {.name = "Tile", .vendor = "Tile", .adv_cmp = {0x02, 0x01, 0x06, 0x0D}}};

static void task_tracker_timer();
static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record);
static void handle_bt_gapc_events(esp_gap_ble_cb_event_t event_type,
                                  esp_ble_gap_cb_param_t* param);

void trackers_scanner_start() {
  // #if !defined(CONFIG_TRACKERS_SCANNER_DEBUG)
  //   esp_log_level_set(TAG_BLE_CLIENT_MODULE, ESP_LOG_NONE);
  // #endif

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
    if (trackers_scan_duration >= TRACKER_SCAN_DURATION) {
      ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
      trackers_scanner_stop();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void trackers_scanner_stop() {
  trackers_scanner_active = false;
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
  if (trackers_scan_timer_task != NULL) {
    ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
    vTaskSuspend(trackers_scan_timer_task);
  }
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
  trackers_scan_duration = 0;
  vTaskDelete(NULL);
  // TODO: When this is called, the BLE stopping bricks the device
  // bt_gattc_task_stop();
  ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers task stopped");
}

static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record) {
  for (int i = 0; i < 3; i++) {
    if (scan_rst->scan_rst.ble_adv[0] == trackers[i].adv_cmp[0] &&
        scan_rst->scan_rst.ble_adv[1] == trackers[i].adv_cmp[1] &&
        scan_rst->scan_rst.ble_adv[2] == trackers[i].adv_cmp[2] &&
        scan_rst->scan_rst.ble_adv[3] == trackers[i].adv_cmp[3]) {
      tracker_record->is_tracker = true;
      tracker_record->name = trackers[i].name;
      tracker_record->vendor = trackers[i].vendor;
      tracker_record->adv_data_length = scan_rst->scan_rst.adv_data_len;
      tracker_record->rssi = scan_rst->scan_rst.rssi;
      memcpy(tracker_record->mac_address, scan_rst->scan_rst.bda, 6);
      memcpy(tracker_record->adv_data, scan_rst->scan_rst.ble_adv,
             sizeof(tracker_record->adv_data));

      ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Trackers found");
      ESP_LOGI(TAG_BLE_CLIENT_MODULE, "Address: %02X:%02X:%02X:%02X:%02X:%02X",
               scan_rst->scan_rst.bda[5], scan_rst->scan_rst.bda[4],
               scan_rst->scan_rst.bda[3], scan_rst->scan_rst.bda[2],
               scan_rst->scan_rst.bda[1], scan_rst->scan_rst.bda[0]);
      ESP_LOGI(TAG_BLE_CLIENT_MODULE,
               "ADV data %d:", scan_rst->scan_rst.adv_data_len);
      ESP_LOG_BUFFER_HEX(TAG_BLE_CLIENT_MODULE, &scan_rst->scan_rst.ble_adv,
                         scan_rst->scan_rst.adv_data_len);
      ESP_LOGI(TAG_BLE_CLIENT_MODULE, " ");
      break;
    }
  }
}

void trackers_scanner_add_tracker_profile(tracker_profile_t** profiles,
                                          uint16_t* num_profiles,
                                          tracker_profile_t new_profile) {
  *profiles =
      realloc(*profiles, (*num_profiles + 1) * sizeof(tracker_profile_t));
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
