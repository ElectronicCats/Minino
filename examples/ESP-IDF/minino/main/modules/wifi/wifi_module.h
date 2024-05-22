#pragma once

#include "esp_wifi.h"
#include "keyboard_module.h"
#include "menu_screens_modules.h"

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_deauth_begin();

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_analizer_begin();

/**
 * @brief Stop the wifi module
 *
 */
void wifi_module_exit(void);

/**
 * @brief Callback to show the summary of the wifi analizer
 *
 * @param pcap_file The pcap file
 *
 * @return void
 */
void wifi_module_analizer_summary_cb(FILE* pcap_file);

/**
 * @brief Update the channel items array
 *
 * @return void
 */
void wifi_module_update_channel_options();

/**
 * @brief State machine for the wifi module
 *
 * @param button_pressed The button pressed
 */
void wifi_module_keyboard_cb(button_event_t button_pressed);
