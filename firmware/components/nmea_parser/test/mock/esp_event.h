// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef const char* esp_event_base_t;
typedef void (*esp_event_handler_t)(void* event_handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data);

#define ESP_EVENT_DECLARE_BASE(base) extern const char* base
#define ESP_EVENT_DEFINE_BASE(base) const char* base = #base
