#include "OTA.h"
#include <stdio.h>
#include "wifi_ap.h"

void OTA_init() {
  wifi_app_start();
}

void OTA_set_show_event_cb(ota_show_event_cb_t* cb) {
  ota_show_event_cb = cb;
}
