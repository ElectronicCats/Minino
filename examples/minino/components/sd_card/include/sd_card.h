#pragma once

#include <stdbool.h>
#include "esp_err.h"

void sd_card_init();
void sd_card_mount();
void sd_card_unmount();
bool sd_card_is_mounted();
esp_err_t sd_card_create_file(const char* path);
