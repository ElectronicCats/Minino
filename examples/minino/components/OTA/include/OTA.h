#pragma once
#include <stdbool.h>
#include <stdio.h>

#define CURRENT_FW_VERSION "V1.1.0.1"

typedef enum {
  OTA_SHOW_PROGRESS_EVENT,
  OTA_SHOW_START_EVENT,
  OTA_SHOW_RESULT_EVENT
} ota_show_events_t;

typedef void (*ota_show_event_cb_t)(uint8_t, void*);

bool is_ota_running = false;

void OTA_init();
void OTA_set_show_event_cb(ota_show_event_cb_t* cb);
void OTA_call_show_event_cb(uint8_t event, void* context);