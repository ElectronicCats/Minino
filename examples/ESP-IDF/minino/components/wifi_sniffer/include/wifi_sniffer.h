#pragma once

#include <stdint.h>

#include "cmd_pcap.h"
#include "cmd_sniffer.h"

#define CHANNEL_MAX 14  // US = 11, EU = 13, JP = 14

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
 * @brief Exit the wifi sniffer
 *
 * @return void
 */
void wifi_sniffer_exit();

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
