#include "services/gattcmd_service.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static void gattcmd_scan_gap_cb(esp_gap_ble_cb_event_t event,
                                esp_ble_gap_cb_param_t* param);
static void gattcmd_scan_gattc_cb(esp_gattc_cb_event_t event,
                                  esp_gatt_if_t gattc_if,
                                  esp_ble_gattc_cb_param_t* param);
static void gattcmd_enum_gattc_profile_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x60,
    .scan_window = 0x40,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE};

struct gattc_profile_inst {
  esp_gattc_cb_t gattc_cb;
  uint16_t gattc_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_start_handle;
  uint16_t service_end_handle;
  uint16_t char_handle;
  esp_bd_addr_t remote_bda;
};

static struct gattc_profile_inst enum_gl_profile_tab[GATTCMD_ENUM_PROFILE] = {
    [GATTCMD_ENUM_APP_ID] =
        {
            .gattc_cb = gattcmd_enum_gattc_profile_event_handler,
            .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
                                              ESP_GATT_IF_NONE */
        },
};

static void gattcmd_enum_gattc_profile_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param) {
  esp_ble_gattc_cb_param_t* p_data = (esp_ble_gattc_cb_param_t*) param;
  switch (event) {
    case ESP_GATTC_REG_EVT:
      esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
      if (scan_ret) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "set scan params error, error code = %x",
                 scan_ret);
      }
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (p_data->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Service search failed, status %x",
                 p_data->search_cmpl.status);
        break;
      }
      if (p_data->search_cmpl.searched_service_source ==
          ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
      } else if (p_data->search_cmpl.searched_service_source ==
                 ESP_GATT_SERVICE_FROM_NVS_FLASH) {
      } else {
        ESP_LOGI(GATTCMD_ENUM_TAG, "Unknown service source");
      }
      break;
    default:
      break;
  }
}

static void gattcmd_scan_gap_cb(esp_gap_ble_cb_event_t event,
                                esp_ble_gap_cb_param_t* param) {
  uint8_t adv_name_len = 0;
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      // the unit of the duration is second
      uint32_t duration = 30;
      esp_ble_gap_start_scanning(duration);
      break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      // scan start complete event to indicate scan start successfully or failed
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Scanning start failed, status %x",
                 param->scan_start_cmpl.status);
        break;
      }
      ESP_LOGI(GATTCMD_ENUM_TAG, "Scanning start successfully");
      printf("|___________________________|____________________|\n");
      printf("|\t ADDRESS \t\t|\t RSSI \t\t|\n");
      break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*) param;
      switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
          esp_ble_resolve_adv_data_by_type(
              scan_result->scan_rst.ble_adv,
              scan_result->scan_rst.adv_data_len +
                  scan_result->scan_rst.scan_rsp_len,
              ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
          printf("|___________________________|____________________|\n");
          printf("|\t" ESP_BD_ADDR_STR "  |\t %d \t\t|\n",
                 ESP_BD_ADDR_HEX(scan_result->scan_rst.bda),
                 scan_result->scan_rst.rssi);
          break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
          break;
        default:
          break;
      }
      break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Scanning stop failed, status %x",
                 param->scan_stop_cmpl.status);
        break;
      }
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Advertising stop failed, status %x",
                 param->adv_stop_cmpl.status);
        break;
      }
      ESP_LOGI(GATTCMD_ENUM_TAG, "Advertising stop successfully");
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
      ESP_LOGI(GATTCMD_ENUM_TAG,
               "Packet length update, status %d, rx %d, tx %d",
               param->pkt_data_length_cmpl.status,
               param->pkt_data_length_cmpl.params.rx_len,
               param->pkt_data_length_cmpl.params.tx_len);
      break;
    default:
      break;
  }
}

static void gattcmd_scan_gattc_cb(esp_gattc_cb_event_t event,
                                  esp_gatt_if_t gattc_if,
                                  esp_ble_gattc_cb_param_t* param) {
  /* If event is register event, store the gattc_if for each profile */
  if (event == ESP_GATTC_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      enum_gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
    } else {
      ESP_LOGI(GATTCMD_ENUM_TAG, "reg app failed, app_id %04x, status %d",
               param->reg.app_id, param->reg.status);
      return;
    }
  }

  /* If the gattc_if equal to profile A, call profile A cb handler,
   * so here call each profile's callback */
  do {
    int idx;
    for (idx = 0; idx < GATTCMD_ENUM_PROFILE; idx++) {
      if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a
                                             certain gatt_if, need to call every
                                             profile cb function */
          gattc_if == enum_gl_profile_tab[idx].gattc_if) {
        if (enum_gl_profile_tab[idx].gattc_cb) {
          enum_gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
        }
      }
    }
  } while (0);
}

void gattcmd_scan_begin(void) {
  // register the  callback function to the gap module
  esp_err_t ret = esp_ble_gap_register_callback(gattcmd_scan_gap_cb);
  if (ret) {
    ESP_LOGE(GATTCMD_ENUM_TAG, "%s gap register failed, error code = %x",
             __func__, ret);
    return;
  }

  // register the callback function to the gattc module
  ret = esp_ble_gattc_register_callback(gattcmd_scan_gattc_cb);
  if (ret) {
    ESP_LOGE(GATTCMD_ENUM_TAG, "%s gattc register failed, error code = %x",
             __func__, ret);
    return;
  }

  ret = esp_ble_gattc_app_register(GATTCMD_ENUM_APP_ID);
  if (ret) {
    ESP_LOGE(GATTCMD_ENUM_TAG, "%s gattc app register failed, error code = %x",
             __func__, ret);
  }
  esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
  if (local_mtu_ret) {
    ESP_LOGE(GATTCMD_ENUM_TAG, "set local  MTU failed, error code = %x",
             local_mtu_ret);
  }
}