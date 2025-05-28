#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "gattcmd_module.h"
#include "services/gattcmd_service.h"

#define GATTC_TAG               "GATTC_DEMO"
#define REMOTE_SERVICE_UUID     0x00FF
#define REMOTE_NOTIFY_CHAR_UUID 0xFF01
#define PROFILE_NUM             1
#define PROFILE_A_APP_ID        0
#define INVALID_HANDLE          0

static esp_bd_addr_t target;
static bool initialized = false;

void gattcmd_begin(void) {
  // Initialize NVS.
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret) {
    ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s", __func__,
             esp_err_to_name(ret));
    return;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret) {
    ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__,
             esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_init();
  if (ret) {
    ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s", __func__,
             esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_enable();
  if (ret) {
    ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s", __func__,
             esp_err_to_name(ret));
    return;
  }
}

void parse_address_colon(const char* str, uint8_t addr[6]) {
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1], &addr[2],
         &addr[3], &addr[4], &addr[5]);
}

void parse_address_raw(const char* str, uint8_t addr[6]) {
  for (int i = 0; i < 6; i++) {
    sscanf(str + 2 * i, "%2hhx", &addr[i]);
  }
}

int hex_string_to_bytes(const char* hex_str, uint8_t* output, size_t max_len) {
  size_t len = strlen(hex_str);

  if (len % 2 != 0)
    return -1;

  size_t bytes_len = len / 2;
  if (bytes_len > max_len)
    return -2;

  for (size_t i = 0; i < bytes_len; i++) {
    sscanf(hex_str + 2 * i, "%2hhx", &output[i]);
  }

  return bytes_len;
}

void gattcmd_module_gatt_write(char* gatt, char* value) {
  uint8_t gatt_addr[32];
  uint8_t value_hex[32];

  int len1 = hex_string_to_bytes(gatt, gatt_addr, sizeof(gatt_addr));
  int len2 = hex_string_to_bytes(value, value_hex, sizeof(value_hex));

  esp_log_buffer_hex(GATTC_TAG, gatt_addr, len1);
  esp_log_buffer_hex(GATTC_TAG, value_hex, len2);

  ESP_LOGI(GATTC_TAG, "Writting GATT: %s - value: %s", gatt, value);
}

void gattcmd_module_set_remote_address(char* saddress) {
  parse_address_colon(saddress, target);
}

void gattcmd_module_enum_client(char* saddress) {
  if (initialized) {
    ESP_LOGI(GATTC_TAG, "Stopping scan");
    gattcmd_scan_stop();
    gattcmd_enum_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }

  gattcmd_enum_begin(saddress);
}

void gattcmd_module_scan_client() {
  if (initialized) {
    ESP_LOGI(GATTC_TAG, "Stopping scan");
    gattcmd_scan_stop();
    gattcmd_enum_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }

  gattcmd_scan_begin();
}