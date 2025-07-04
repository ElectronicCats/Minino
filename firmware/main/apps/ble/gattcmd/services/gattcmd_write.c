#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "services/gattcmd_service.h"

#include "services/gattcmd_service.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define GATTC_WRITE_TAG "GATT_WRITE"

static esp_bd_addr_t target_bda;
static esp_gattc_char_elem_t* char_elem_result = NULL;
static esp_gattc_descr_elem_t* descr_elem_result = NULL;
static bool connect = false;
static bool get_server = false;

static uint16_t gatt_target_uuid = 0x0000;
static uint8_t gatt_target_value[254];
static uint16_t gatt_target_value_len = 0;
static uint16_t count = 0;

static void gattcmd_write_gap_cb(esp_gap_ble_cb_event_t event,
                                 esp_ble_gap_cb_param_t* param);
static void gattcmd_write_gattc_cb(esp_gattc_cb_event_t event,
                                   esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t* param);
static void gattcmd_write_gattc_profile_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

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
            .gattc_cb = gattcmd_write_gattc_profile_event_handler,
            .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
                                              ESP_GATT_IF_NONE */
        },
};

int hex_string_to_bytes(const char* hex_str, uint8_t* out_buf, size_t max_len) {
  size_t len = strlen(hex_str);
  if (len % 2 != 0 || len / 2 > max_len) {
    return -1;
  }

  for (size_t i = 0; i < len / 2; ++i) {
    char byte_str[3] = {hex_str[2 * i], hex_str[2 * i + 1], '\0'};
    out_buf[i] = (uint8_t) strtol(byte_str, NULL, 16);
  }
  return (int) (len / 2);
}

static void parse_address_colon_w(const char* str, uint8_t addr[6]) {
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1], &addr[2],
         &addr[3], &addr[4], &addr[5]);
}

void gattcmd_write(char* saddress, uint16_t target_uuid, char* value_str) {
  parse_address_colon_w(saddress, target_bda);
  gatt_target_value_len = hex_string_to_bytes(value_str, gatt_target_value,
                                              sizeof(gatt_target_value));
  gatt_target_uuid = target_uuid;
  if (char_elem_result) {
    for (int i = 0; i < count; i++) {
      if (char_elem_result[i].uuid.uuid.uuid16 == gatt_target_uuid) {
        esp_err_t res = esp_ble_gattc_write_char(
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].gattc_if,
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].conn_id,
            char_elem_result[i].char_handle, gatt_target_value_len,
            gatt_target_value, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        if (res == ESP_OK) {
          printf("[" GATTC_WRITE_TAG "] Write done\n");
        } else {
          printf("[" GATTC_WRITE_TAG "] Write error: %d\n", res);
        }
        break;
      }
    }
  }
}

static void gattcmd_write_gattc_profile_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param) {
  esp_ble_gattc_cb_param_t* p_data = (esp_ble_gattc_cb_param_t*) param;
  switch (event) {
    case ESP_GATTC_REG_EVT:
      esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
      if (scan_ret) {
        ESP_LOGE(GATTC_WRITE_TAG, "set scan params error, error code = %x",
                 scan_ret);
      }
      break;
    case ESP_GATTC_CONNECT_EVT: {
      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].conn_id =
          p_data->connect.conn_id;
      memcpy(enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].remote_bda,
             p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
      esp_err_t mtu_ret =
          esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
      if (mtu_ret) {
        ESP_LOGE(GATTC_WRITE_TAG, "Config MTU error, error code = %x", mtu_ret);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGE(GATTC_WRITE_TAG, "Open failed, status %d",
                 p_data->open.status);
        break;
      }
      break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
      if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(GATTC_WRITE_TAG, "Service discover failed, status %d",
                 param->dis_srvc_cmpl.status);
        break;
      }
      /* Discover all the services */
      esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id,
                                   NULL);
      break;
    case ESP_GATTC_SEARCH_RES_EVT: {
      get_server = true;
      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle =
          p_data->search_res.start_handle;
      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle =
          p_data->search_res.end_handle;
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (p_data->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(GATTC_WRITE_TAG, "Service search failed, status %x",
                 p_data->search_cmpl.status);
        break;
      }
      if (p_data->search_cmpl.searched_service_source ==
          ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
      } else if (p_data->search_cmpl.searched_service_source ==
                 ESP_GATT_SERVICE_FROM_NVS_FLASH) {
      } else {
        ESP_LOGI(GATTC_WRITE_TAG, "Unknown service source");
      }

      if (get_server) {
        count = 0;
        uint16_t offset = 0;
        esp_gatt_status_t status = esp_ble_gattc_get_attr_count(
            gattc_if, p_data->search_cmpl.conn_id, ESP_GATT_DB_CHARACTERISTIC,
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle,
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle,
            INVALID_HANDLE, &count);
        if (status != ESP_GATT_OK) {
          ESP_LOGE(GATTC_WRITE_TAG, "esp_ble_gattc_get_attr_count error");
          break;
        }
        if (count > 0) {
          char_elem_result = (esp_gattc_char_elem_t*) malloc(
              sizeof(esp_gattc_char_elem_t) * count);
          if (!char_elem_result) {
            ESP_LOGE(GATTC_WRITE_TAG, "gattc no mem");
            break;
          }
          status = esp_ble_gattc_get_all_char(
              gattc_if, p_data->search_cmpl.conn_id,
              enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle,
              enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle,
              char_elem_result, &count, offset);
          if (status != ESP_GATT_OK) {
            ESP_LOGE(GATTC_WRITE_TAG, "esp_ble_gattc_get_char_by_uuid error");
            free(char_elem_result);
            char_elem_result = NULL;
            break;
          }
          for (int i = 0; i < count; i++) {
            if (char_elem_result[i].uuid.uuid.uuid16 == gatt_target_uuid) {
              esp_err_t res = esp_ble_gattc_write_char(
                  gattc_if, p_data->connect.conn_id,
                  char_elem_result[i].char_handle, gatt_target_value_len,
                  gatt_target_value, ESP_GATT_WRITE_TYPE_RSP,
                  ESP_GATT_AUTH_REQ_NONE);
              if (res == ESP_OK) {
                printf("[" GATTC_WRITE_TAG "] Write done\n");
              } else {
                printf("[" GATTC_WRITE_TAG "] Write error: %d\n", res);
              }
              break;
            }
          }
          descr_elem_result = NULL;
        } else {
          ESP_LOGE(GATTC_WRITE_TAG, "no char found");
        }
      }
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      connect = false;
      get_server = false;
      free(char_elem_result);
      ESP_LOGI(GATTC_WRITE_TAG,
               "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
               ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda),
               p_data->disconnect.reason);
      break;
    default:
      break;
  }
}

static void gattcmd_write_gap_cb(esp_gap_ble_cb_event_t event,
                                 esp_ble_gap_cb_param_t* param) {
  uint8_t* adv_name = NULL;
  uint8_t adv_name_len = 0;
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      uint32_t duration = 30;
      esp_ble_gap_start_scanning(duration);
      break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTC_WRITE_TAG, "Scanning start failed, status %x",
                 param->scan_start_cmpl.status);
        break;
      }
      break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*) param;
      switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
          adv_name = esp_ble_resolve_adv_data_by_type(
              scan_result->scan_rst.ble_adv,
              scan_result->scan_rst.adv_data_len +
                  scan_result->scan_rst.scan_rsp_len,
              ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
          if (adv_name_len > 0)
            if (adv_name != NULL) {
              if (memcmp(target_bda, scan_result->scan_rst.bda, 6) == 0) {
                if (connect == false) {
                  connect = true;
                  esp_ble_gap_stop_scanning();
                  printf("[" GATTC_WRITE_TAG
                         "] Connecting with device: " ESP_BD_ADDR_STR "\n",
                         ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));
                  esp_ble_gattc_open(
                      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].gattc_if,
                      scan_result->scan_rst.bda,
                      scan_result->scan_rst.ble_addr_type, true);
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
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      printf("Scan Stoped\n");
      if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTC_WRITE_TAG, "Scanning stop failed, status %x",
                 param->scan_stop_cmpl.status);
        break;
      }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTC_WRITE_TAG, "Advertising stop failed, status %x",
                 param->adv_stop_cmpl.status);
        break;
      }
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
      ESP_LOGI(GATTC_WRITE_TAG, "Packet length update, status %d, rx %d, tx %d",
               param->pkt_data_length_cmpl.status,
               param->pkt_data_length_cmpl.params.rx_len,
               param->pkt_data_length_cmpl.params.tx_len);
      break;
    default:
      break;
  }
}

static void gattcmd_write_gattc_cb(esp_gattc_cb_event_t event,
                                   esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t* param) {
  /* If event is register event, store the gattc_if for each profile */
  if (event == ESP_GATTC_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      enum_gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
    } else {
      ESP_LOGI(GATTC_WRITE_TAG, "reg app failed, app_id %04x, status %d",
               param->reg.app_id, param->reg.status);
      if (param->reg.status == 128)
        esp_restart();
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

static void parse_address_colon(const char* str, uint8_t addr[6]) {
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1], &addr[2],
         &addr[3], &addr[4], &addr[5]);
}

void gattcmd_write_begin(char* saddress,
                         uint16_t target_uuid,
                         char* value_str) {
  parse_address_colon(saddress, target_bda);
  gatt_target_value_len = hex_string_to_bytes(value_str, gatt_target_value,
                                              sizeof(gatt_target_value));
  gatt_target_uuid = target_uuid;
  // register the  callback function to the gap module
  esp_err_t ret = esp_ble_gap_register_callback(gattcmd_write_gap_cb);
  if (ret) {
    ESP_LOGE(GATTC_WRITE_TAG, "%s gap register failed, error code = %x",
             __func__, ret);
    return;
  }

  // register the callback function to the gattc module
  ret = esp_ble_gattc_register_callback(gattcmd_write_gattc_cb);
  if (ret) {
    ESP_LOGE(GATTC_WRITE_TAG, "%s gattc register failed, error code = %x",
             __func__, ret);
    return;
  }

  ret = esp_ble_gattc_app_register(GATTCMD_ENUM_APP_ID);
  if (ret) {
    ESP_LOGE(GATTC_WRITE_TAG, "%s gattc app register failed, error code = %x",
             __func__, ret);
  }
  esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
  if (local_mtu_ret) {
    ESP_LOGE(GATTC_WRITE_TAG, "set local  MTU failed, error code = %x",
             local_mtu_ret);
  }
}

void gattcmd_write_stop() {
  if (connect) {
    esp_ble_gattc_close(enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].gattc_if,
                        enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].conn_id);
    esp_ble_gap_disconnect(target_bda);
  }

  esp_ble_gattc_app_unregister(
      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].gattc_if);
  esp_ble_gattc_cache_clean(target_bda);

  connect = false;
  get_server = false;
  enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].gattc_if = ESP_GATT_IF_NONE;
}