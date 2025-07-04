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
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "gattcmd_module.h"
#include "services/gattcmd_service.h"

#define GATTC_TAG "GATTC_DEMO"

static esp_bd_addr_t target;
static bool initialized = false;

void gattcmd_begin(void) {
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_err_t ret = esp_bt_controller_init(&bt_cfg);
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

void gattcmd_module_gatt_write(char* saddress, char* gatt, char* value) {
  uint16_t uuid = hex_string_to_uint16(gatt);

  if (initialized) {
    gattcmd_scan_stop();
    gattcmd_enum_stop();
    printf("Writting GATT: %s - %s - value: %s\n", saddress, gatt, value);
    gattcmd_write(saddress, uuid, value);
  } else {
    initialized = true;
    gattcmd_begin();
    printf("Writting GATT: %s - %s - value: %s\n", saddress, gatt, value);
    gattcmd_write_begin(saddress, uuid, value);
  }
}

void gattcmd_module_set_remote_address(char* saddress) {
  parse_address_colon(saddress, target);
}

void gattcmd_module_enum_client(char* saddress) {
  if (initialized) {
    gattcmd_module_stop_workers();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  printf("gattcmd_enum_begin\n");
  gattcmd_enum_begin(saddress);
}

void gattcmd_module_scan_client() {
  if (initialized) {
    gattcmd_module_stop_workers();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  gattcmd_scan_begin();
}

void gattcmd_module_stop_workers() {
  gattcmd_scan_stop();
  gattcmd_enum_stop();
  gattcmd_write_stop();
}