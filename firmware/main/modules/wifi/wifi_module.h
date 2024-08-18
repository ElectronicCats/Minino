#pragma once

#include "esp_wifi.h"
#include "keyboard_module.h"
#include "menu_screens_modules.h"

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_begin();

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_module_analizer_begin();

void wifi_module_init_sniffer();

/**
 * @brief Stop the wifi module
 *
 */
void wifi_module_exit(void);
void wifi_module_analyzer_run_exit();
void wifi_module_analizer_exit();
void wifi_module_analizer_summary_exit();
void wifi_module_analyzer_destination_exit();

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
 * @brief Update the destination items array
 *
 * @return void
 */
void wifi_module_update_destination_options();

/**
 * @brief State machine for the wifi module
 *
 * @param button_pressed The button pressed
 */
void wifi_module_keyboard_cb(uint8_t button_name, uint8_t button_event);
