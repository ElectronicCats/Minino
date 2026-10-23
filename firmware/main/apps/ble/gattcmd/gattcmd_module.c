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
#include "gap_dispatcher.h"
#include "gattcmd_module.h"
#include "radio_lease.h"
#include "services/gattcmd_service.h"

#define GATTC_TAG "GATTC_DEMO"

static esp_bd_addr_t target;
static volatile bool initialized = false;

/* Returns true when the BLE stack claim was really taken. Returning only a
 * log used to let every command run its esp_ble_* calls against a stack that
 * was never brought up, and to release a claim that was never held on exit. */
static bool gattcmd_shared_begin(void) {
  esp_err_t ret;

  /* Bring up the controller and Bluedroid through the central lease,
     so we don't steal the stack from bt_spam, ble_hid, bt_gattc, etc. */
  ret = radio_lease_ble_acquire();
  if (ret != ESP_OK) {
    ESP_LOGE(GATTC_TAG, "could not acquire the BLE stack: %s",
             esp_err_to_name(ret));
    return false;
  }
  return true;
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

static bool gattcmd_ensure_ble_up(void) {
  if (initialized) {
    return true;
  }
  if (!gattcmd_shared_begin()) {
    return false;
  }
  initialized = true;
  return true;
}

void gattcmd_module_gatt_write(char* saddress, char* gatt, char* value) {
  if (!gattcmd_ensure_ble_up()) {
    printf("GATT write skipped: the BLE stack is not available\n");
    return;
  }
  printf("Writting GATT: %s - %s - value: %s\n", saddress, gatt, value);
  uint16_t uuid = hex_string_to_uint16(gatt);
  gattcmd_write_begin(saddress, uuid, value);
}

void gattcmd_module_set_remote_address(char* saddress) {
  parse_address_colon(saddress, target);
}

void gattcmd_module_enum_client(char* saddress) {
  if (!gattcmd_ensure_ble_up()) {
    printf("gattcmd enum skipped: the BLE stack is not available\n");
    return;
  }
  printf("gattcmd_enum_begin\n");
  gattcmd_enum_begin(saddress);
}

void gattcmd_module_scan_client() {
  if (!gattcmd_ensure_ble_up()) {
    printf("gattcmd scan skipped: the BLE stack is not available\n");
    return;
  }
  gattcmd_scan_begin();
}

void gattcmd_module_recon(const char* bt_addr) {
  if (!gattcmd_ensure_ble_up()) {
    printf("gattcmd recon skipped: the BLE stack is not available\n");
    return;
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
  if (initialized) {
    initialized = false;
    radio_lease_ble_release();
  }
}
