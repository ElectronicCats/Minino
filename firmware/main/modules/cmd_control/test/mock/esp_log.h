#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGD(tag, fmt, ...) do { (void)(tag); } while (0)
#define esp_log_level_set(tag, level) do { (void)(tag); (void)(level); } while (0)
#define ESP_LOG_NONE 0
#define ESP_ERROR_CHECK(x) do { int _err = (x); (void)_err; } while(0)
