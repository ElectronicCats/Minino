#pragma once
#include <stdint.h>

#define CHIP_FEATURE_WIFI_BGN   (1 << 0)
#define CHIP_FEATURE_BT         (1 << 1)
#define CHIP_FEATURE_BLE        (1 << 2)
#define CHIP_FEATURE_IEEE802154 (1 << 3)
#define CHIP_FEATURE_EMB_FLASH  (1 << 4)

typedef struct {
  uint32_t features;
  uint8_t cores;
  uint32_t revision;
} esp_chip_info_t;

void esp_chip_info(esp_chip_info_t* out_info);
