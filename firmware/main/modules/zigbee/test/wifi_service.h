#pragma once
#define WIFI_MODE_STA 1
static inline void wifi_service_deinit(void) {}
static inline void wifi_service_init(int m) {
  (void) m;
}
