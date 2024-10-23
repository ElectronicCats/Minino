#pragma once

#include "esp_wifi.h"
#include "wifi_sniffer.h"

#define WIFI_SCREENS_MODULE_H
#define TAG_WIFI_SCREENS_MODULE "module:wifi_screens"

/**
 * @brief Display the wifi module scanned screen
 *
 * @param ap_records The pointer to the scanned AP records
 * @param scanned_records The number of scanned records
 * @param current_option The current option selected
 */
void wifi_screens_module_display_scanned_networks(wifi_ap_record_t* ap_records,
                                                  int scanned_records,
                                                  int current_option);

/**
 * @brief Display the wifi sniffer progress screen
 *
 * @param sniffer The sniffer runtime
 *
 * @return void
 */
void wifi_screens_module_display_sniffer_cb(sniffer_runtime_t* sniffer);

/**
 * @brief Display the wifi sniffer animation screen
 *
 * @param sniffer The sniffer runtime
 *
 * @return void
 */
void wifi_screens_display_sniffer_animation_task();

/**
 * @brief Create the wifi sniffer task
 *
 * @return void
 */
void wifi_screens_module_create_sniffer_task();

/**
 * @brief Start the wifi sniffer animation
 *
 * @return void
 */
void wifi_screens_sniffer_animation_start();

/**
 * @brief Stop the wifi sniffer animation
 *
 * @return void
 */
void wifi_screens_sniffer_animation_stop();

void wifi_screeens_show_sd_not_supported();

void wifi_screeens_show_sd_not_found();