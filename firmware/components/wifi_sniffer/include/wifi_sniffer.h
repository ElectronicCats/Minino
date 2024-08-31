#pragma once

#include <stdint.h>

#include "cmd_pcap.h"
#include "cmd_sniffer.h"

#define CHANNEL_MAX 14  // US = 11, EU = 13, JP = 14

typedef enum {
  WIFI_SNIFFER_DESTINATION_SD = 0,
  WIFI_SNIFFER_DESTINATION_INTERNAL,
} pcap_destination_t;

/**
 * @brief Initialize the wifi module
 *
 * @return void
 */
void wifi_sniffer_begin();

/**
 * @brief Start the wifi sniffer
 *
 * @return void
 */
void wifi_sniffer_start();

/**
 * @brief Stop the wifi sniffer
 *
 * @return void
 */
void wifi_sniffer_stop();

/**
 * @brief Close the pcap file
 *
 * @return void
 */
void wifi_sniffer_close_file();

/**
 * @brief Load the summary
 *
 * @return void
 */
void wifi_sniffer_load_summary();

/**
 * @brief Get the channel
 *
 * @return uint8_t
 */
uint8_t wifi_sniffer_get_channel();

/**
 * @brief Set the channel
 *
 * @param channel The channel
 *
 * @return void
 */
void wifi_sniffer_set_channel(uint8_t new_channel);

/**
 * @brief Check if the destination is SD
 *
 * @return bool
 */
bool wifi_sniffer_is_destination_sd();

/**
 * @brief Check if the destination is internal
 *
 * @return bool
 */
bool wifi_sniffer_is_destination_internal();

/**
 * @brief Set the destination to SD
 *
 * @param destination The destination
 *
 * @return void
 */
void wifi_sniffer_set_destination_sd();

/**
 * @brief Set the destination to internal
 *
 * @param destination The destination
 *
 * @return void
 */
void wifi_sniffer_set_destination_internal();
