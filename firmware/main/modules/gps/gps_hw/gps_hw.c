#include "gps_hw.h"

#include "driver/gpio.h"

#include "preferences.h"

#define GPS_ON_OFF_PIN 8

static bool gps_enabled = false;

void gps_hw_init() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << GPS_ON_OFF_PIN);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  gps_enabled = preferences_get_bool(GPS_ENABLED_MEM, false);

  gpio_set_level(GPS_ON_OFF_PIN, gps_enabled);
}

void gps_hw_on() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_set_level(GPS_ON_OFF_PIN, 1);
  gps_enabled = true;
}

void gps_hw_off() {
#ifndef CONFIG_GPS_ENABLED
  return;
#endif
  gpio_set_level(GPS_ON_OFF_PIN, 0);
  gps_enabled = false;
}

bool get_gps_enabled() {
  return gps_enabled;
}