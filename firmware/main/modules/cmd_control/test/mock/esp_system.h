#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "sdkconfig.h"

#define ESP_OK 0
#define ESP_FAIL -1
typedef int esp_err_t;

#define ESP_ERROR_CHECK(x) do { int _err = (x); (void)_err; } while(0)
#define MALLOC_CAP_8BIT 0

uint32_t heap_caps_get_free_size(uint32_t caps);
uint32_t heap_caps_get_minimum_free_size(uint32_t caps);
uint32_t esp_get_free_heap_size(void);

typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON,
    ESP_RST_EXT,
    ESP_RST_SW,
    ESP_RST_PANIC,
    ESP_RST_INT_WDT,
    ESP_RST_TASK_WDT,
    ESP_RST_WDT,
    ESP_RST_DEEPSLEEP,
    ESP_RST_BROWNOUT,
    ESP_RST_SDIO,
    ESP_RST_USB,
    ESP_RST_JTAG,
    ESP_RST_EFUSE,
    ESP_RST_PWR_GLITCH,
    ESP_RST_CPU_LOCKUP,
} esp_reset_reason_t;

esp_reset_reason_t esp_reset_reason(void);
void esp_restart(void);
