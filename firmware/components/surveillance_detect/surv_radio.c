// SPDX-License-Identifier: GPL-3.0-or-later
// surv_radio.c — stub; la implementacion real llega en la Task 10.
#include "surv_radio.h"

esp_err_t surv_radio_start(surv_profile_t p, bool active_scan) {
  (void) p;
  (void) active_scan;
  return ESP_OK;
}

void surv_radio_stop(void) {}

uint8_t surv_radio_current_channel(void) {
  return 0;
}
