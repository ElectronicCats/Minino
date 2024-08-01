
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#ifndef BT_GATTS_H
  #define BT_GATTS_H
  #define TAG_BT_GATTS    "bt_gatts"
  #define DEVICE_NAME     "EC_BLE_SERVER"
  #define DEVICE_PROFILES 1
  #define DEVICE_PROFILE  0

  #define GATTS_SERVICE_UUID_USERNAME   0x00FF
  #define GATTS_CHAR_UUID_USERNAME      0xFF01
  #define GATTS_DESCR_UUID_USERNAME     0x3333
  #define GATTS_NUM_HANDLE_USERNAME     4
  #define BOARD_MANUFACTURER_DATA_LEN   17
  #define GATTS_DEVICE_CHAR_VAL_LEN_MAX 0x80  // 128 bytes
  #define PREPARE_BUF_MAX_SIZE          1024
  #define adv_config_flag               (1 << 0)
  #define scan_rsp_config_flag          (1 << 1)

uint8_t test_manufacturer[BOARD_MANUFACTURER_DATA_LEN] = {0x12, 0x23, 0x45,
                                                          0x56};
uint8_t ble_server_adv_service_uuid128[32] = {
    // first uuid, 16bit, [12],[13] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xEE,
    0x00,
    0x00,
    0x00,
    // second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xFF,
    0x00,
    0x00,
    0x00,
};
uint8_t char_value[] = {0x61, 0x70, 0x70, 0x73, 0x65, 0x63};

typedef struct {
  uint8_t* prepare_buf;
  int prepare_len;
} prepare_type_env_t;

/**
 * @brief Struct contain the GATT Server profile instance
 */
struct gatts_profile_inst {
  esp_gatts_cb_t gatts_cb;
  uint16_t gatts_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_handle;
  esp_gatt_srvc_id_t service_id;
  uint16_t char_handle;
  esp_bt_uuid_t char_uuid;
  esp_gatt_perm_t perm;
  esp_gatt_char_prop_t property;
  uint16_t descr_handle;
  esp_bt_uuid_t descr_uuid;
};

typedef struct {
  char* device_name;
  uint8_t* manufacturer_data;
} bt_server_props_t;

struct bt_server_props_t {
  char* device_name;
  uint8_t* manufacturer_data;
};

typedef struct {
  esp_ble_adv_data_t adv_data;
  esp_ble_adv_data_t scan_rsp_data;
  esp_ble_adv_params_t adv_params;
  esp_attr_value_t char_val;
  bt_server_props_t bt_props;
} gatts_adv_params_t;

struct gatts_adv_params_t {
  esp_ble_adv_data_t adv_data;
  esp_ble_adv_data_t scan_rsp_data;
  esp_ble_adv_params_t adv_params;
  esp_attr_value_t char_val;
  bt_server_props_t bt_props;
};

typedef struct {
  void (*handler_gatt_cb)(esp_gatts_cb_event_t event_type,
                          esp_ble_gatts_cb_param_t* param);
  void (*handler_gap_cb)(esp_gap_ble_cb_event_t event_type,
                         esp_ble_gap_cb_param_t* param);
} bt_server_event_cb_t;

struct gatts_profile_inst gatts_profile_tab[DEVICE_PROFILES];
prepare_type_env_t a_prepare_write_env;

/**
 * @brief GATT Server write event handler
 *
 * @param gatts_if The GATT interface
 * @param prepare_write_env The prepare write environment
 * @param param The GATT Server parameters
 *
 * @return void
 */
void ble_server_write_event(esp_gatt_if_t gatts_if,
                            prepare_type_env_t* prepare_write_env,
                            esp_ble_gatts_cb_param_t* param);

/**
 * @brief GATT Server event handler
 *
 * @param event The GATT Server event
 * @param gatts_if The GATT interface
 * @param param The GATT Server parameters
 *
 * @return void
 */
void ble_server_gatt_profiles_event_handler(esp_gatts_cb_event_t event,
                                            esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t* param);
/**
 * @brief Send data to the remote device
 *
 * @param data The data to send
 * @param length The length of the data
 *
 * @return void
 */
void ble_server_send_data(uint8_t* data, int length);
/**
 * @brief Initialize the GATT Server profile
 *
 * @return void
 */
void bt_gatts_task_begin(void);
/**
 * @brief Stop the GATT Server profile
 *
 * @return void
 */
void bt_gatts_task_stop(void);

esp_ble_adv_params_t bt_gatts_set_default_ble_adv_params();
esp_ble_adv_data_t bt_gatts_set_default_ble_scan_rsp();
esp_ble_adv_data_t bt_gatts_set_default_ble_adv_data();
esp_attr_value_t bt_gatts_set_default_char_val();
void bt_gatts_set_ble_adv_data_params();
void bt_gatts_set_cb(bt_server_event_cb_t event_cb);
#endif  // BT_GATTS_H
