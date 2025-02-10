#pragma once

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

esp_err_t resistor_detector(gpio_num_t gpio_num);