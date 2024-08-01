
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#ifndef BT_GATTC_H
  #define BT_GATTC_H
  #define TAG_BT_GATTC            "bt_gattc"
  #define REMOTE_BOARD            "EC_APPSECPWN_RED"
  #define REMOTE_SERVICE_UUID     0x00FF
  #define REMOTE_NOTIFY_CHAR_UUID 0xFF01
  #define DEVICE_PROFILES         1
  #define DEVICE_PROFILE          0
  #define INVALID_HANDLE          0
  #define MAX_REMOTE_DEVICE_NAME  20
  #define SCAN_DURATION           60

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

/**
 * @brief Struct contain the GATT Client profile instance
 */
typedef struct {
  esp_gattc_cb_t gattc_cb;
  uint16_t gattc_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_start_handle;
  uint16_t service_end_handle;
  uint16_t char_handle;
  esp_bd_addr_t remote_bda;
} gattc_profile_inst;

typedef struct {
  esp_bt_uuid_t remote_filter_service_uuid;
  esp_bt_uuid_t remote_filter_char_uuid;
  esp_bt_uuid_t notify_descr_uuid;
  esp_ble_scan_params_t ble_scan_params;
} gattc_scan_params_t;

struct gattc_scan_params_t {
  esp_bt_uuid_t remote_filter_service_uuid;
  esp_bt_uuid_t remote_filter_char_uuid;
  esp_bt_uuid_t notify_descr_uuid;
  esp_ble_scan_params_t ble_scan_params;
};

typedef struct {
  void (*handler_gattc_cb)(esp_gattc_cb_event_t event_type,
                           esp_ble_gattc_cb_param_t* param);
  void (*handler_gapc_cb)(esp_gap_ble_cb_event_t event_type,
                          esp_ble_gap_cb_param_t* param);
} bt_client_event_cb_t;

/**
 * @brief GATT Client event handler
 *
 * @param event The GATT Client event
 * @param gattc_if The GATT interface
 *
 * @return void
 */
void ble_client_esp_gap_cb(esp_gap_ble_cb_event_t event,
                           esp_ble_gap_cb_param_t* param);
/**
 * @brief GATT Client event handler
 *
 * @param event The GATT Client event
 * @param gattc_if The GATT interface
 * @param param The GATT Client parameters
 *
 * @return void
 */
void ble_client_esp_gattc_cb(esp_gattc_cb_event_t event,
                             esp_gatt_if_t gattc_if,
                             esp_ble_gattc_cb_param_t* param);
/**
 * @brief Send data to the remote device
 *
 * @param data The data to send
 * @param length The length of the data
 *
 * @return void
 */
void ble_client_send_data(uint8_t* data, int length);
/**
 * @brief GATT Client event handler
 *
 * @param event The GATT Client event
 * @param gattc_if The GATT interface
 * @param param The GATT Client parameters
 *
 * @return void
 */
void ble_client_gattc_event_handler(esp_gattc_cb_event_t event,
                                    esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t* param);

/**
 * @brief Initialize the GATT Client profile
 *
 * @return void
 */
void bt_gattc_task_begin(void);
/**
 * @brief Stop the GATT Client profile
 *
 * @return void
 */
void bt_gattc_task_stop(void);

esp_bt_uuid_t bt_gattc_set_default_ble_filter_service_uuid();
esp_bt_uuid_t bt_gattc_set_default_ble_filter_char_uuid();
esp_bt_uuid_t bt_gattc_set_default_ble_notify_descr_uuid();
esp_ble_scan_params_t bt_gattc_set_default_ble_scan_params();
void bt_gattc_set_ble_scan_params(gattc_scan_params_t* scan_params);
void bt_gattc_set_cb(bt_client_event_cb_t event_cb);
void bt_gattc_set_remote_device_name(const char* device_name);
#endif  // BT_GATTC_H
