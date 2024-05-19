#pragma once

#include "cmd_pcap.h"
#include "cmd_sniffer.h"

#define SIMPLE_SNIFFER_H
#define CHANNEL_MAX 13  // US = 11, EU = 13, JP = 14

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
