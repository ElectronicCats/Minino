/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "ble_hidd_main.h"
#include "radio_lease.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#include "esp_hidd_prf_api.h"
#include "gap_dispatcher.h"
#include "hidd_le_prf_int.h"
#include "hid_dev.h"

/**
 * Brief:
 * This example Implemented BLE HID device profile related functions, in which
 * the HID device has 4 Reports (1 is mouse, 2 is keyboard and LED, 3 is
 * Consumer Devices, 4 is Vendor devices). Users can choose different reports
 * according to their own application scenarios. BLE HID profile inheritance and
 * USB HID class.
 */

/**
 * Note:
 * 1. Win10 does not support vendor report , So SUPPORT_REPORT_VENDOR is always
 * set to FALSE, it defines in hidd_le_prf_int.h
 * 2. Update connection parameters are not allowed during iPhone HID encryption,
 * slave turns off the ability to automatically update connection parameters
 * during encryption.
 * 3. After our HID device is connected, the iPhones write 1 to the Report
 * Characteristic Configuration Descriptor, even if the HID encryption is not
 * completed. This should actually be written 1 after the HID encryption is
 * completed. we modify the permissions of the Report Characteristic
 * Configuration Descriptor to `ESP_GATT_PERM_READ |
 * ESP_GATT_PERM_WRITE_ENCRYPTED`. if you got `GATT_INSUF_ENCRYPTION` error,
 * please ignore.
 */

#define HID_DEMO_TAG          "HID_DEMO"
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

static uint16_t hid_conn_id = 0;
static bool sec_conn = false;
static bool is_connected = false;
static bool s_hid_started = false; /* true while HID holds a BLE lease claim */
static hid_event_callback_f hid_event_callback = NULL;

static void hidd_event_callback(esp_hidd_cb_event_t event,
                                esp_hidd_cb_param_t* param);

static char* HIDD_DEVICE_NAME = "MININO_HID";
static uint8_t hidd_service_uuid128[] = {
    /* LSB
       <-------------------------------------------------------------------------------->
       MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static uint8_t s_adv_config_done = 0;
#define ADV_CONFIG_FLAG      (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x0006,  // slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010,  // slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,    // HID Generic
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_data_t hidd_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x03c0,    // HID Generic
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x30,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void hidd_event_callback(esp_hidd_cb_event_t event,
                                esp_hidd_cb_param_t* param) {
  switch (event) {
    case ESP_HIDD_EVENT_REG_FINISH: {
      if (param->init_finish.state == ESP_HIDD_INIT_OK) {
        esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&hidd_adv_data);
        esp_ble_gap_config_adv_data(&hidd_scan_rsp_data);
      }
      break;
    }
    case ESP_BAT_EVENT_REG: {
      break;
    }
    case ESP_HIDD_EVENT_DEINIT_FINISH:
      break;
    case ESP_HIDD_EVENT_BLE_CONNECT: {
      ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT (conn_id=%d)",
               param->connect.conn_id);
      hid_conn_id = param->connect.conn_id;
      is_connected = true;
      /* Link connected at GATT level; pairing/bonding/encryption happens next.
       * Do not notify UI until ESP_GAP_BLE_AUTH_CMPL_EVT succeeds. */
      break;
    }
    case ESP_HIDD_EVENT_BLE_DISCONNECT: {
      is_connected = false;
      sec_conn = false;
      ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
      esp_ble_gap_start_advertising(&hidd_adv_params);
      if (hid_event_callback != NULL) {
        hid_event_callback(false);
      }
      break;
    }
    case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
      ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT",
               __func__);
      ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data,
                         param->vendor_write.length);
      break;
    }
    case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
      ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
      ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->led_write.data,
                         param->led_write.length);
      break;
    }
    default:
      break;
  }
  return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t* param);
static void hid_drop_bond(const esp_bd_addr_t bd_addr);

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t* param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      s_adv_config_done |= ADV_CONFIG_FLAG;
      if (s_adv_config_done == (ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG)) {
        ESP_LOGI(HID_DEMO_TAG, "Adv data and Scan Rsp set, starting advertising...");
        esp_ble_gap_start_advertising(&hidd_adv_params);
      }
      break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      s_adv_config_done |= SCAN_RSP_CONFIG_FLAG;
      if (s_adv_config_done == (ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG)) {
        ESP_LOGI(HID_DEMO_TAG, "Scan Rsp and Adv data set, starting advertising...");
        esp_ble_gap_start_advertising(&hidd_adv_params);
      }
      break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
      for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
        ESP_LOGD(HID_DEMO_TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
      }
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    case ESP_GAP_BLE_NC_REQ_EVT:
      ESP_LOGI(HID_DEMO_TAG, "ESP_GAP_BLE_NC_REQ_EVT, num_val = %lu",
               (unsigned long) param->ble_security.key_notif.passkey);
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
      break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
      ESP_LOGI(HID_DEMO_TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT, passkey = %lu",
               (unsigned long) param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
      ESP_LOGI(HID_DEMO_TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 0);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr,
             sizeof(esp_bd_addr_t));
      ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",
               (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) +
                   bd_addr[3],
               (bd_addr[4] << 8) + bd_addr[5]);
      ESP_LOGI(HID_DEMO_TAG, "address type = %d",
               param->ble_security.auth_cmpl.addr_type);
      ESP_LOGI(HID_DEMO_TAG, "pair status = %s",
               param->ble_security.auth_cmpl.success ? "success" : "fail");
      if (param->ble_security.auth_cmpl.success) {
        sec_conn = true;
        is_connected = true;
        ESP_LOGI(HID_DEMO_TAG, "Authentication / pairing completed successfully");
        if (hid_event_callback != NULL) {
          hid_event_callback(true);
        }
      } else {
        sec_conn = false;
        is_connected = false;
        ESP_LOGE(HID_DEMO_TAG, "Pairing failed, reason = 0x%x",
                 param->ble_security.auth_cmpl.fail_reason);
        /* A failed pairing usually means our bond for this peer is stale (the
         * classic HOGP "PIN or Key Missing"): the host keeps its copy of the
         * key, must not be asked to pair afresh while we hold the old record.
         * Drop our side so the next attempt is a clean pairing, sparing the
         * user the manual "forget device" dance. */
        hid_drop_bond(bd_addr);
        if (hid_event_callback != NULL) {
          hid_event_callback(false);
        }
      }
      break;
    }
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
      ESP_LOGI(HID_DEMO_TAG, "bond removed (status = %d)",
               param->remove_bond_dev_cmpl.status);
      break;
    default:
      break;
  }
}

/* Removes *our* copy of the bond record for bd_addr, if any. */
static void hid_drop_bond(const esp_bd_addr_t bd_addr) {
  int num = esp_ble_get_bond_device_num();
  if (num <= 0) {
    return;
  }
  esp_ble_bond_dev_t* dev_list = malloc(sizeof(esp_ble_bond_dev_t) * num);
  if (dev_list == NULL) {
    return;
  }
  if (esp_ble_get_bond_device_list(&num, dev_list) == ESP_OK) {
    for (int i = 0; i < num; i++) {
      if (memcmp(bd_addr, dev_list[i].bd_addr, sizeof(esp_bd_addr_t)) == 0) {
        ESP_LOGW(HID_DEMO_TAG, "deleting stale bond for %02x:%02x:%02x:%02x:%02x:%02x",
                 bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4],
                 bd_addr[5]);
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
        break;
      }
    }
  }
  free(dev_list);
}

/* Every action button funnels through here so that "did the host actually get
   the report?" has exactly one answer. The UI needs it: the notification
   windows used to be drawn unconditionally, so pressing Volume Up with no host
   attached flashed a confirmation for something that was silently dropped. */
static bool hid_send_consumer(const char* action, uint16_t key_cmd,
                              bool press) {
  if (!is_connected || !sec_conn) {
    ESP_LOGW(HID_DEMO_TAG, "%s ignored: connected=%d, secured=%d", action,
             (int)is_connected, (int)sec_conn);
    return false;
  }
  ESP_LOGI(HID_DEMO_TAG, "%s: %s (conn_id=%d, sec=%d)", action,
           press ? "PRESS" : "RELEASE", hid_conn_id, (int)sec_conn);
  esp_hidd_send_consumer_value(hid_conn_id, key_cmd, press);
  return true;
}

bool ble_hid_volume_up(bool press) {
  return hid_send_consumer("ble_hid_volume_up", HID_CONSUMER_VOLUME_UP, press);
}

bool ble_hid_volume_down(bool press) {
  return hid_send_consumer("ble_hid_volume_down", HID_CONSUMER_VOLUME_DOWN,
                           press);
}

bool ble_hid_play(bool press) {
  return hid_send_consumer("ble_hid_play", HID_CONSUMER_PLAY, press);
}

bool ble_hid_pause(bool press) {
  return hid_send_consumer("ble_hid_pause", HID_CONSUMER_PAUSE, press);
}

bool ble_hid_stop(bool press) {
  return hid_send_consumer("ble_hid_stop", HID_CONSUMER_STOP, press);
}

bool ble_hid_mute(bool press) {
  return hid_send_consumer("ble_hid_mute", HID_CONSUMER_MUTE, press);
}

bool ble_hid_next_track(bool press) {
  return hid_send_consumer("ble_hid_next_track", HID_CONSUMER_SCAN_NEXT_TRK,
                           press);
}

bool ble_hid_prev_track(bool press) {
  return hid_send_consumer("ble_hid_prev_track", HID_CONSUMER_SCAN_PREV_TRK,
                           press);
}

void ble_hid_register_callback(hid_event_callback_f callback) {
  hid_event_callback = callback;
}

void ble_hid_get_device_name(char* device_name) {
  strlcpy(device_name, HIDD_DEVICE_NAME, 32);
}

void ble_hid_get_device_mac(uint8_t* mac) {
  esp_read_mac(mac, ESP_MAC_BT);
}

void ble_hid_begin() {
  esp_err_t ret;
  s_adv_config_done = 0;

  if (s_hid_started) {
    ESP_LOGW(HID_DEMO_TAG, "ble_hid_begin() while already started, ignoring");
    return;
  }

  /* Brings up the controller and Bluedroid, and holds a BLE claim for as long as
     the profile is configured: the surveillance detector's window hand-over must
     not deinit the stack underneath a paired host. */
  if ((ret = radio_lease_ble_acquire()) != ESP_OK) {
    ESP_LOGE(HID_DEMO_TAG, "could not acquire the BLE stack: %d", ret);
    return;
  }
  /* HID is an advertising app: while it is enabled no scanner may grab the
   * radio, otherwise start_advertising() is rejected until the scan ends or
   * the app appears dead to the phone. */
  radio_lease_ble_block_scan();
  s_hid_started = true;

  if ((ret = esp_hidd_profile_init()) != ESP_OK) {
    ESP_LOGE(HID_DEMO_TAG, "%s init hid profile failed", __func__);
    ble_hid_end();
    return;
  }

  /* Register the GAP handler through the central dispatcher. Without a
     subscriber nothing consumes ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT, so
     esp_ble_gap_start_advertising() is never called and the HID device is
     configured but never advertises. */
  ret = gap_dispatcher_register(gap_event_handler);
  if (ret != ESP_OK) {
    ESP_LOGE(HID_DEMO_TAG, "gap_dispatcher_register failed: %d", ret);
    ble_hid_end();
    return;
  }

  esp_hidd_register_callbacks(hidd_event_callback);

  /* set the security iocap & auth_req & key size & init key response key
   * parameters to the stack*/
  esp_ble_auth_req_t auth_req =
      ESP_LE_AUTH_REQ_SC_BOND;  // LE Secure Connections bonding
  esp_ble_io_cap_t iocap =
      ESP_IO_CAP_NONE;    // set the IO capability to No output No input
  uint8_t key_size = 16;  // the key size should be 7~16 bytes
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,
                                 &auth_option, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req,
                                 sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap,
                                 sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size,
                                 sizeof(uint8_t));
  /* If your BLE device act as a Slave, the init_key means you hope which types
  of key of the master should distribute to you, and the response key means
  which key you can distribute to the Master; If your BLE device act as a
  master, the response key means you hope which types of key of the slave should
  distribute to you, and the init key means which key you can distribute to the
  slave. */
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key,
                                 sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key,
                                 sizeof(uint8_t));
  ESP_LOGI(HID_DEMO_TAG, "bonding mode 0x%02x (0x%02x = LE Secure Connections)",
           (unsigned) auth_req, (unsigned) ESP_LE_AUTH_REQ_SC_BOND);
  /* Same host, new stack bring-up: report what survived in NVS, this is the
   * log the user checks when a re-enable fails to reconnect without
   * forgetting the device. */
  int bond_num = esp_ble_get_bond_device_num();
  ESP_LOGI(HID_DEMO_TAG, "bonded devices in NVS: %d", bond_num);
  if (bond_num > 0) {
    esp_ble_bond_dev_t* bond_list = malloc(sizeof(esp_ble_bond_dev_t) * bond_num);
    if (bond_list != NULL) {
      if (esp_ble_get_bond_device_list(&bond_num, bond_list) == ESP_OK) {
        for (int i = 0; i < bond_num; i++) {
          esp_bd_addr_t* b = &bond_list[i].bd_addr;
          ESP_LOGI(HID_DEMO_TAG, "  bonded: %02x:%02x:%02x:%02x:%02x:%02x",
                   (*b)[0], (*b)[1], (*b)[2], (*b)[3], (*b)[4], (*b)[5]);
        }
      }
      free(bond_list);
    }
  }
}

void ble_hid_end(void) {
  if (!s_hid_started) {
    return;
  }
  s_hid_started = false;
  esp_ble_gap_stop_advertising();
  /* If a host is still connected, close the ACL first: deleting the service
   * and unregistering the app under a live link produces disconnect events on
   * an already-deleted profile and wedges the next enable. */
  if (is_connected && hidd_le_env.gatt_if != ESP_GATT_IF_NONE) {
    ESP_LOGI(HID_DEMO_TAG, "closing live ACL before teardown");
    esp_ble_gatts_close(hidd_le_env.gatt_if, hid_conn_id);
  }
  gap_dispatcher_unregister(gap_event_handler);
  esp_hidd_profile_deinit();
  /* profile_deinit() only queues stop/delete/unregister on the BTC task: the
   * stack must not be torn down until the queued work has run, otherwise a
   * second enable deadlocks on a half-registered SRS. */
  esp_hidd_wait_for_unreg(500);
  sec_conn = false;
  is_connected = false;
  hid_conn_id = 0;
  hid_event_callback = NULL;
  s_adv_config_done = 0;
  radio_lease_ble_release();
  /* Scanning apps may use the radio again only after the stack claim is
   * back. */
  radio_lease_ble_unblock_scan();
}
