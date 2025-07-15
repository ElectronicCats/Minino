#include "services/gattcmd_service.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static esp_bd_addr_t target_bda;
static esp_gattc_char_elem_t* char_elem_result = NULL;
static esp_gattc_descr_elem_t* descr_elem_result = NULL;
static bool connect = false;
static bool get_server = false;
static bool draw_headers = true;

static void gattcmd_enum_gap_cb(esp_gap_ble_cb_event_t event,
                                esp_ble_gap_cb_param_t* param);
static void gattcmd_enum_gattc_cb(esp_gattc_cb_event_t event,
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
            .gattc_cb = gattcmd_enum_gattc_profile_event_handler,
            .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
                                              ESP_GATT_IF_NONE */
        },
};

static void get_char_properties(uint8_t properties,
                                char* output,
                                size_t output_size) {
  output[0] = '\0';
  if (properties & ESP_GATT_CHAR_PROP_BIT_BROADCAST)
    strcat(output, "BROADCAST, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_READ)
    strcat(output, "READ, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR)
    strcat(output, "WRITE_NR, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_WRITE)
    strcat(output, "WRITE, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
    strcat(output, "NOTIFY, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_INDICATE)
    strcat(output, "INDICATE, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_AUTH)
    strcat(output, "AUTH, ");
  if (properties & ESP_GATT_CHAR_PROP_BIT_EXT_PROP)
    strcat(output, "EXT_PROP, ");
  size_t len = strlen(output);
  if (len >= 2)
    output[len - 2] = '\0';
}

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
    case ESP_GATTC_CONNECT_EVT: {
      enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].conn_id =
          p_data->connect.conn_id;
      memcpy(enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].remote_bda,
             p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
      esp_err_t mtu_ret =
          esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
      if (mtu_ret) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Config MTU error, error code = %x",
                 mtu_ret);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Open failed, status %d",
                 p_data->open.status);
        break;
      }
      break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
      if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Service discover failed, status %d",
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
      if (draw_headers) {
        draw_headers = false;
        printf(
            "\n|______________|________________________________________|_______"
            "_________|____________________|\n");
        printf(
            "| Handles        | Service > Characteristics > Descriptors| "
            "Properties     |\t Data             |\n");
        printf(
            "|________________|________________________________________|_______"
            "_________|____________________|\n");
      }
      printf(
          "| %04x -> %04x   | %04x (Service)                   \t|\t  "
          "\t|\t\t\t|\n",
          p_data->search_res.start_handle, p_data->search_res.end_handle,
          p_data->search_res.srvc_id.uuid.uuid.uuid16);
      break;
    }
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
      printf(
          "|________________|_______________________________________|__________"
          "_____|____________________|\n");
      printf("| %04x -> %04x \t|\t\t\t\t\t|\t\t|\t\t\t|\n",
             enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle,
             enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle);

      if (get_server) {
        uint16_t count = 0;
        uint16_t offset = 0;
        esp_gatt_status_t status = esp_ble_gattc_get_attr_count(
            gattc_if, p_data->search_cmpl.conn_id, ESP_GATT_DB_CHARACTERISTIC,
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle,
            enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle,
            INVALID_HANDLE, &count);
        if (status != ESP_GATT_OK) {
          ESP_LOGE(GATTCMD_ENUM_TAG, "esp_ble_gattc_get_attr_count error");
          break;
        }
        if (count > 0) {
          char_elem_result = (esp_gattc_char_elem_t*) malloc(
              sizeof(esp_gattc_char_elem_t) * count);
          if (!char_elem_result) {
            ESP_LOGE(GATTCMD_ENUM_TAG, "gattc no mem");
            break;
          }
          status = esp_ble_gattc_get_all_char(
              gattc_if, p_data->search_cmpl.conn_id,
              enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_start_handle,
              enum_gl_profile_tab[GATTCMD_ENUM_APP_ID].service_end_handle,
              char_elem_result, &count, offset);
          if (status != ESP_GATT_OK) {
            ESP_LOGE(GATTCMD_ENUM_TAG, "esp_ble_gattc_get_char_by_uuid error");
            free(char_elem_result);
            char_elem_result = NULL;
            break;
          }
          for (int i = 0; i < count; i++) {
            char prop_str[128];
            get_char_properties(char_elem_result[i].properties, prop_str,
                                sizeof(prop_str));
            printf("|\t %04x  \t| %04x (Char) \t\t\t\t| %s |\t\t\t|\n",
                   char_elem_result[i].char_handle,
                   char_elem_result[i].uuid.uuid.uuid16, prop_str);
            if (char_elem_result[i].properties & ESP_GATT_CHAR_PROP_BIT_READ) {
              esp_ble_gattc_read_char(gattc_if, p_data->search_cmpl.conn_id,
                                      char_elem_result[i].char_handle,
                                      ESP_GATT_AUTH_REQ_NONE);
            }

            uint16_t desc_count = 0;
            status = esp_ble_gattc_get_attr_count(
                gattc_if, p_data->search_cmpl.conn_id, ESP_GATT_DB_DESCRIPTOR,
                char_elem_result[i].char_handle,
                char_elem_result[i].char_handle + 1,
                char_elem_result[i].char_handle, &desc_count);
            if (status != ESP_GATT_OK) {
              ESP_LOGE(GATTCMD_ENUM_TAG,
                       "esp_ble_gattc_get_attr_count (descr) error");
              continue;
            }
            if (desc_count > 0) {
              descr_elem_result = (esp_gattc_descr_elem_t*) malloc(
                  sizeof(esp_gattc_descr_elem_t) * desc_count);
              if (!descr_elem_result) {
                ESP_LOGE(GATTCMD_ENUM_TAG, "Failed malloc");
                continue;
              }
              status = esp_ble_gattc_get_all_descr(
                  gattc_if, p_data->search_cmpl.conn_id,
                  char_elem_result[i].char_handle, descr_elem_result,
                  &desc_count, 0);
              if (status != ESP_GATT_OK) {
                ESP_LOGE(GATTCMD_ENUM_TAG, "esp_ble_gattc_get_all_descr error");
                free(descr_elem_result);
                descr_elem_result = NULL;
                continue;
              }
              for (int j = 0; j < desc_count; j++) {
                printf(
                    "| %04x \t\t| %04x (Descr)                "
                    "\t\t|\t\t|\t\t|\n",
                    descr_elem_result[j].handle,
                    descr_elem_result[j].uuid.uuid.uuid16);
                esp_ble_gattc_read_char_descr(
                    gattc_if, p_data->search_cmpl.conn_id,
                    descr_elem_result[j].handle, ESP_GATT_AUTH_REQ_NONE);
              }
              free(descr_elem_result);
              descr_elem_result = NULL;
            }
          }
          free(char_elem_result);
        } else {
          ESP_LOGE(GATTCMD_ENUM_TAG, "no char found");
        }
        printf(
            "|_______________|______________________________________|__________"
            "______|____________________|\n");
      }
      break;
    case ESP_GATTC_READ_CHAR_EVT:
      if (p_data->read.status != ESP_GATT_OK) {
        break;
      }
      printf("|\t %04x\t |\t\t\t\t\t |\t\t| %s |\n", p_data->read.handle,
             p_data->read.value);
      break;
    case ESP_GATTC_READ_DESCR_EVT:
      if (p_data->read.status != ESP_GATT_OK) {
        break;
      }
      printf("| %04x| %04x \t\t| %s |\n", p_data->read.handle,
             p_data->read.handle, p_data->read.value);
      break;
    case ESP_GATTC_SRVC_CHG_EVT: {
      esp_bd_addr_t bda;
      memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
      ESP_LOGI(GATTCMD_ENUM_TAG, "Service change from " ESP_BD_ADDR_STR "",
               ESP_BD_ADDR_HEX(bda));
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
      connect = false;
      get_server = false;
      printf("Disconnected, remote: \n" ESP_BD_ADDR_STR,
             ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda));
      ESP_LOGI(GATTCMD_ENUM_TAG,
               "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
               ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda),
               p_data->disconnect.reason);
      break;
    default:
      break;
  }
}

static void gattcmd_enum_gap_cb(esp_gap_ble_cb_event_t event,
                                esp_ble_gap_cb_param_t* param) {
  uint8_t* adv_name = NULL;
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
      if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Scanning stop failed, status %x",
                 param->scan_stop_cmpl.status);
        break;
      }
      printf("Scanning stop\n");
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(GATTCMD_ENUM_TAG, "Advertising stop failed, status %x",
                 param->adv_stop_cmpl.status);
        break;
      }
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

static void gattcmd_enum_gattc_cb(esp_gattc_cb_event_t event,
                                  esp_gatt_if_t gattc_if,
                                  esp_ble_gattc_cb_param_t* param) {
  /* If event is register event, store the gattc_if for each profile */
  if (event == ESP_GATTC_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      enum_gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
    } else {
      ESP_LOGI(GATTCMD_ENUM_TAG, "reg app failed, app_id %04x, status %d",
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

void gattcmd_enum_begin(char* saddress) {
  parse_address_colon(saddress, target_bda);
  // register the  callback function to the gap module
  esp_err_t ret = esp_ble_gap_register_callback(gattcmd_enum_gap_cb);
  if (ret) {
    ESP_LOGE(GATTCMD_ENUM_TAG, "%s gap register failed, error code = %x",
             __func__, ret);
    return;
  }

  // register the callback function to the gattc module
  ret = esp_ble_gattc_register_callback(gattcmd_enum_gattc_cb);
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

void gattcmd_enum_stop() {
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