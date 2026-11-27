#pragma once
#include "freertos/FreeRTOS.h"
#define SD_CARD_PATH "/sdcard"
static inline esp_err_t sd_card_mount(void) {
  return ESP_FAIL;
}
static inline esp_err_t sd_card_create_dir(const char* d) {
  (void) d;
  return ESP_OK;
}
static inline esp_err_t sd_card_append_to_file(const char* f, char* b) {
  (void) f;
  (void) b;
  return ESP_OK;
}
