#pragma once

#include <stdio.h>

/**
 * @brief Display toogle button pressed
 *
 * @return void
 */
void zigbee_screens_module_toogle_pressed();

/**
 * @brief Display toogle button released
 *
 * @return void
 */
void zigbee_screens_module_toggle_released();

/**
 * @brief Display "Creating network"
 *
 * @return void
 */
void zigbee_screens_module_creating_network();

/**
 * @brief Display "Creating network failed"
 *
 * @return void
 */
void zigbee_screens_module_creating_network_failed();

/**
 * @brief Display "Waiting for devices"
 *
 * @param uint8_t dots
 *
 * @return void
 */
void zigbee_screens_module_waiting_for_devices();

/**
 * @brief Display "No devices found"
 *
 * @return void
 */
void zigbee_screens_module_no_devices_found();

/**
 * @brief Display "Closing network"
 *
 * @return void
 */
void zigbee_screens_module_closing_network();

void zigbee_screens_module_display_status(uint8_t status);

/////////////////////////////////////////////////////////////////////////

void zigbee_screens_display_device_ad(void);
void zigbee_screens_display_scanning_animation(void);
void zigbee_screens_display_scanning_text(int count);
void zigbee_screens_display_zigbee_sniffer_text();
