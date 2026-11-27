#pragma once
#define ESP_LOGE(tag, fmt, ...) ((void) (tag))
#define ESP_LOGW(tag, fmt, ...) ((void) (tag))
#define ESP_LOGI(tag, fmt, ...) ((void) (tag))
static inline unsigned long esp_log_timestamp(void) {
  return 1000UL;
}
