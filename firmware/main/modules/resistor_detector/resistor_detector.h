#pragma once

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

bool resistor_detector(gpio_num_t gpio_num);