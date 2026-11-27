#pragma once

#include <stdint.h>

#define TAG_ZIGBEE_MODULE "zigbee_module:main"

void zigbee_module_begin(int app_selected);
void zigbee_module_switch_enter(void);
void zigbee_module_light_enter(void);
void zigbee_module_sniffer_enter(void);
void zigbee_module_sniffer_exit(void);
