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

#define GATTC_TAG "GATTC_DEMO"

static esp_bd_addr_t target;
static volatile bool initialized = false;

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

static bool parse_address_colon(const char* str, uint8_t addr[6]) {
  return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr[0], &addr[1],
                &addr[2], &addr[3], &addr[4], &addr[5]) == 6;
}

void parse_address_raw(const char* str, uint8_t addr[6]) {
  for (int i = 0; i < 6; i++) {
    sscanf(str + 2 * i, "%2hhx", &addr[i]);
  }
}

uint16_t hex_string_to_uint16(const char* hex_str) {
  return (uint16_t) strtol(hex_str, NULL, 16);
}

void gattcmd_module_gatt_write(char* saddress, char* gatt, char* value) {
  if (initialized) {
    gattcmd_scan_stop();
    gattcmd_enum_stop();
    gattcmd_write_stop();
    gattcmd_recon_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  printf("Writting GATT: %s - %s - value: %s\n", saddress, gatt, value);
  uint16_t uuid = hex_string_to_uint16(gatt);
  gattcmd_write_begin(saddress, uuid, value);
}

void gattcmd_module_set_remote_address(char* saddress) {
  parse_address_colon(saddress, target);
}

void gattcmd_module_enum_client(char* saddress) {
  if (initialized) {
    gattcmd_scan_stop();
    gattcmd_enum_stop();
    gattcmd_write_stop();
    gattcmd_recon_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  printf("gattcmd_enum_begin\n");
  gattcmd_enum_begin(saddress);
}

void gattcmd_module_scan_client() {
  if (initialized) {
    gattcmd_scan_stop();
    gattcmd_enum_stop();
    gattcmd_write_stop();
    gattcmd_recon_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  gattcmd_scan_begin();
}

void gattcmd_module_recon(const char* bt_addr) {
  if (initialized) {
    gattcmd_scan_stop();
    gattcmd_enum_stop();
    gattcmd_write_stop();
    gattcmd_recon_stop();
  } else {
    initialized = true;
    gattcmd_begin();
  }
  bool match_all = (bt_addr == NULL);
  esp_bd_addr_t target_addr;

  if (!match_all) {
    if (!parse_address_colon(bt_addr, target_addr)) {
      ESP_LOGE(GATTC_TAG, "Invalid BT address: %s", bt_addr);
      return;
    }
    ESP_LOGI(GATTC_TAG, "Starting recon for specific device: " ESP_BD_ADDR_STR,
             ESP_BD_ADDR_HEX(target_addr));
  } else {
    ESP_LOGI(GATTC_TAG, "Starting recon for all nearby devices");
  }
  gattcmd_recon_begin(bt_addr);
}

void gattcmd_module_stop_workers() {
  gattcmd_scan_stop();
  gattcmd_recon_stop();
  gattcmd_write_stop();
  gattcmd_enum_stop();
}