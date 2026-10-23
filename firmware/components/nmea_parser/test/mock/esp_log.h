// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdio.h>

#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5

#define ESP_LOGE(tag, fmt, ...) do {} while(0)
#define ESP_LOGW(tag, fmt, ...) do {} while(0)
#define ESP_LOGI(tag, fmt, ...) do {} while(0)
#define ESP_LOGD(tag, fmt, ...) do {} while(0)

static inline void esp_log_level_set(const char* tag, int level) {
  (void) tag;
  (void) level;
}
