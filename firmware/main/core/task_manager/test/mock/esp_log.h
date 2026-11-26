/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#define ESP_LOG_NONE 0

/* Takes the arguments so the code under test sees them used. */
static inline void mock_log(const char* tag, const char* fmt, ...) {
  (void) tag;
  (void) fmt;
}

#define esp_log_level_set(tag, level) ((void) (tag), (void) (level))
#define ESP_LOGI(tag, ...)            mock_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...)            mock_log(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...)            mock_log(tag, __VA_ARGS__)
