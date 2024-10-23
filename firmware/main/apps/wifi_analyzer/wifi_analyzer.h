#pragma once

#include "esp_wifi.h"
#include "keyboard_module.h"

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

void wifi_module_analyzer_run();

/**
 * @brief Stop the wifi module
 *
 */
void wifi_module_exit(void);
void wifi_module_analyzer_run_exit();
void wifi_module_analyzer_exit();
void wifi_module_analyzer_summary_exit();
void wifi_module_analyzer_destination_exit();

/**
 * @brief Callback to show the summary of the wifi analizer
 *
 * @param pcap_file The pcap file
 *
 * @return void
 */
void wifi_module_analizer_summary_cb(FILE* pcap_file);

void wifi_module_show_analyzer_help();

void wifi_module_analyzer_destination();
void wifi_module_analyzer_channel();