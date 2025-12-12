#pragma once
#include <stdio.h>
#include <string.h>
#include "OTA.h"
#include "oled_screen.h"

void ota_module_screens_show_event(uint8_t event, void* context);
void ota_module_screens_show_help();