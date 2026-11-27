#pragma once

#include <stdint.h>

void zigbee_screens_module_toogle_pressed(void);
void zigbee_screens_module_toggle_released(void);
void zigbee_screens_module_creating_network(void);
void zigbee_screens_module_creating_network_failed(void);
void zigbee_screens_module_waiting_for_devices(void);
void zigbee_screens_module_no_devices_found(void);
void zigbee_screens_module_closing_network(void);
void zigbee_screens_module_display_status(uint8_t status);

/* Zigbee Light screens */
void zigbee_screens_light_joining(void);
void zigbee_screens_light_joining_failed(void);
void zigbee_screens_light_on(void);
void zigbee_screens_light_off(void);
void zigbee_screens_light_leaving(void);
void zigbee_screens_light_disconnected(void);
void zigbee_screens_light_display_status(uint8_t status);

void zigbee_screens_display_device_ad(void);
void zigbee_screens_display_scanning_animation(void);
void zigbee_screens_display_scanning_text(int count);
void zigbee_screens_display_zigbee_sniffer_text(void);
