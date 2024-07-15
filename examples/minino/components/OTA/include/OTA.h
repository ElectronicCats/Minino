#pragma once
#include <stdbool.h>
#include <stdio.h>

typedef enum { OTA_SHOW_PROGRESS_EVENT } ota_show_events_t;

typedef void (*ota_show_event_cb_t)(uint8_t, void*);
ota_show_event_cb_t ota_show_event_cb = NULL;

bool is_ota_running = false;

void OTA_init();
void OTA_set_show_event_cb(ota_show_event_cb_t* cb);