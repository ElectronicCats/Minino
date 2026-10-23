#pragma once
#include <stdint.h>
#include "esp_system.h"

esp_err_t esp_flash_get_size(void *chip, uint32_t *out_size);
