/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MicroSD PCAP Streaming Engine for Minino RF Spectrum.
 * Generates standard libpcap files directly readable in Wireshark.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCAP_PATH_MAX 64

/**
 * @brief Start capturing packets directly to a .pcap file on the MicroSD.
 *
 * Checks if SD is mounted, generates an incremental filename (/sdcard/spectrum_00.pcap),
 * and writes the 24-byte pcap global header.
 *
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if SD not mounted, or ESP_FAIL on file error.
 */
esp_err_t rf_spectrum_pcap_start(void);

/**
 * @brief Stop active PCAP capture and flush file buffers.
 */
void rf_spectrum_pcap_stop(void);

/**
 * @brief Check if PCAP capture is currently active.
 */
bool rf_spectrum_pcap_is_active(void);

/**
 * @brief Write an 802.11 packet to the active PCAP file.
 *
 * @param payload Pointer to raw 802.11 MAC frame
 * @param len Length in bytes
 */
void rf_spectrum_pcap_write_packet(const void* payload, uint32_t len);

/**
 * @brief Get total number of packets written to the current PCAP file.
 */
uint32_t rf_spectrum_pcap_get_packet_count(void);

/**
 * @brief Get the filename of the current or last written PCAP file.
 */
const char* rf_spectrum_pcap_get_filename(void);

#ifdef __cplusplus
}
#endif
