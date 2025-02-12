#include "gps_hw.h"
#include "driver/gpio.h"

#define GPS_ON_OFF_PIN 8

void gps_hw_init() {
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << GPS_ON_OFF_PIN);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  gpio_set_level(GPS_ON_OFF_PIN, 1);
}

void gps_hw_on() {
  gpio_set_level(GPS_ON_OFF_PIN, 1);
}

void gps_hw_off() {
  gpio_set_level(GPS_ON_OFF_PIN, 0);
}