// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

static inline int64_t esp_timer_get_time(void) {
  static int64_t s_mock_time = 1000000;
  s_mock_time += 100000;
  return s_mock_time;
}
