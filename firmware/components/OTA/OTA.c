#include "OTA.h"
#include <stdio.h>
#include "wifi_ap.h"

ota_show_event_cb_t ota_show_event_cb = NULL;

void OTA_init() {
  wifi_app_start();
}

void OTA_set_show_event_cb(ota_show_event_cb_t cb) {
  ota_show_event_cb = cb;
}

void OTA_call_show_event_cb(uint8_t event, void* context) {
  if (event == OTA_SHOW_RESULT_EVENT) {
    is_ota_running = false;
  }
  if (ota_show_event_cb != NULL) {
    ota_show_event_cb(event, context);
  }
}
