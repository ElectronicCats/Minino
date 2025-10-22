#pragma once

#include <stdint.h>
#ifndef IEEE_SNIFFER_H
  #define IEEE_SNIFFER_H
  #define TAG_IEEE_SNIFFER "ieee_sniffer"

  #define IEEE_SNIFFER_CHANNEL_MIN     11
  #define IEEE_SNIFFER_CHANNEL_MAX     26
  #define IEEE_SNIFFER_CHANNEL_DEFAULT 11

  #define LIMIT_PACKETS          1000
  #define FRAME_VERSION_STD_2003 0
  #define FRAME_VERSION_STD_2006 1
  #define FRAME_VERSION_STD_2015 2

  #define FRAME_TYPE_BEACON       (0)
  #define FRAME_TYPE_DATA         (1)
  #define FRAME_TYPE_ACK          (2)
  #define FRAME_TYPE_MAC_COMMAND  (3)
  #define FRAME_TYPE_RESERVED     (4)
  #define FRAME_TYPE_MULTIPURPOSE (5)
  #define FRAME_TYPE_FRAGMENT     (6)
  #define FRAME_TYPE_EXTENDED     (7)

  #define ADDR_MODE_NONE     (0)  // PAN ID and address fields are not present
  #define ADDR_MODE_RESERVED (1)  // Reseved
  #define ADDR_MODE_SHORT    (2)  // Short address (16-bit)
  #define ADDR_MODE_LONG     (3)  // Extended address (64-bit)

  #define FRAME_TYPE_BEACON  (0)
  #define FRAME_TYPE_DATA    (1)
  #define FRAME_TYPE_ACK     (2)
  #define FRAME_TYPE_MAC_CMD (3)

typedef struct mac_fcs {
  uint8_t frameType : 3;
  uint8_t secure : 1;
  uint8_t framePending : 1;
  uint8_t ackReqd : 1;
  uint8_t panIdCompressed : 1;
  uint8_t rfu1 : 1;
  uint8_t sequenceNumberSuppression : 1;
  uint8_t informationElementsPresent : 1;
  uint8_t destAddrType : 2;
  uint8_t frameVer : 2;
  uint8_t srcAddrType : 2;
} mac_fcs_t;

/**
 * @brief Callback to handle the IEEE sniffer
 *
 * @param packets_count The number of packets found
 * @param channel The channel where the record was found
 */
typedef void (*ieee_sniffer_cb_t)(int packets_count, int channel);

/**
 * @brief Register the callback for the IEEE sniffer
 *
 * @param callback The callback to handle the IEEE sniffer
 */
void ieee_sniffer_register_cb(ieee_sniffer_cb_t callback);

/**
 * @brief Begin the IEEE sniffer
 */
void ieee_sniffer_begin(void);

/**
 * @brief Stop the IEEE sniffer
 */
void ieee_sniffer_stop(void);

/**
 * @brief Set the channel for the IEEE sniffer
 *
 * @param channel The channel to set
 */
void ieee_sniffer_set_channel(int channel);

uint8_t ieee_sniffer_get_channel();
int8_t ieee_sniffer_get_rssi();

void ieee_sniffer_channel_hop();

#endif  // IEEE_SNIFFER_H
