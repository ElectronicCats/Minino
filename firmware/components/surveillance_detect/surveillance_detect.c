// SPDX-License-Identifier: GPL-3.0-or-later
// surveillance_detect.c — stub; la implementacion real llega en la Task 9.
#include "surveillance_detect.h"

esp_err_t surv_begin(surv_profile_t profile, bool active_scan) {
  (void) profile;
  (void) active_scan;
  return ESP_OK;
}

void surv_stop(void) {}

void surv_register_cb(surv_event_cb_t cb) {
  (void) cb;
}

uint32_t surv_queue_overflows(void) {
  return 0;
}
