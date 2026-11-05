#pragma once

#include <stdbool.h>
#include <stdint.h>

#define THREAD_FCF_MLE_PACKET    0xd841
#define THREAD_FCF_IEEE_PACKET   0x9869
#define THREAD_FCF_BEACON_PACKET 0xc000
#define PROTOCOL_TYPE_IEEE       "IEEE 802.15.4"
#define PROTOCOL_TYPE_MLE        "MLE"
#define WARTHREAD_BASE_FRAME_LEN 16

typedef struct {
  uint16_t frame_control;
  uint16_t destination_pan;
  uint16_t destination;
  char extd_source_str[24];
  char protocol_type[24];
  uint16_t udp_source_port;
  uint16_t udp_destination_port;
  bool is_mle;
} warthread_parsed_frame_t;

/**
 * @brief Parse an IEEE 802.15.4 / Thread PSDU frame into structured data.
 *
 * @param psdu Raw frame buffer.
 * @param frame_len Total length of the frame in bytes.
 * @param out Pointer to output struct.
 * @return true if frame was successfully parsed, false otherwise.
 */
bool warthread_parse_frame(const uint8_t* psdu,
                           uint8_t frame_len,
                           warthread_parsed_frame_t* out);
