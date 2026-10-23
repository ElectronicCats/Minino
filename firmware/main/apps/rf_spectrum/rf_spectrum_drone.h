/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OpenDroneID / Remote ID passive detection engine for Minino RF Spectrum.
 * Conforms to ASTM F3411 and ASD-STAN prEN 4709-002 direct broadcast standards.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRONE_MAX_ID_LEN 32

typedef struct {
  bool detected;
  char uas_id[DRONE_MAX_ID_LEN];
  uint8_t mac[6];
  uint8_t channel;
  int8_t rssi;
  uint32_t timestamp_ms;
} drone_detection_t;

/**
 * @brief Inspect a raw 802.11 promiscuous packet for OpenDroneID frames.
 *
 * Checks for:
 * 1. Vendor Specific IE 221 (0xDD) with ASD-STAN / ASTM OUI 0xFA, 0x0B, 0xBC
 * 2. French Remote ID OUI 0x6A, 0x5C, 0x35
 * 3. Wi-Fi Alliance NAN Action Frames with Service ID hash 88:69:19:9D:92:09
 *
 * @param[in]  payload Pointer to IEEE 802.11 packet payload
 * @param[in]  len     Length of payload in bytes
 * @param[out] out_info Optional output structure to store extracted drone info
 * @return true if OpenDroneID signature was found, false otherwise
 */
bool rf_spectrum_drone_detect(const uint8_t* payload, uint16_t len, drone_detection_t* out_info);

#ifdef __cplusplus
}
#endif
