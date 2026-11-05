#include "warthread_parser.h"

#include <stdio.h>
#include <string.h>

#define ZB_ADDRESS_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

bool warthread_parse_frame(const uint8_t* psdu,
                           uint8_t frame_len,
                           warthread_parsed_frame_t* out) {
  if (psdu == NULL || out == NULL || frame_len < 3) {
    return false;
  }

  memset(out, 0, sizeof(*out));

  uint8_t position = 0;
  out->frame_control = psdu[position] | ((uint16_t) psdu[position + 1] << 8);
  position += sizeof(uint16_t);

  /* This parser is deliberately limited to the layout used by Thread MLE and
     the 802.15.4 data frames that carry an extended source address. Reject
     other addressing modes instead of reading the following fields at the
     wrong offsets. */
  uint8_t dst_addr_mode = (out->frame_control >> 10) & 0x03;
  uint8_t src_addr_mode = (out->frame_control >> 14) & 0x03;
  bool pan_compressed = (out->frame_control & (1 << 6)) != 0;
  if (dst_addr_mode != 2 || src_addr_mode != 3) {
    return false;
  }

  uint8_t base_len = 3 + 4 + (pan_compressed ? 0 : 2) + 8;
  if (frame_len < base_len) {
    return false;
  }

  position += sizeof(uint8_t);  // Sequence number
  out->destination_pan = psdu[position] | ((uint16_t) psdu[position + 1] << 8);
  position += sizeof(uint16_t);
  out->destination = psdu[position] | ((uint16_t) psdu[position + 1] << 8);
  position += sizeof(uint16_t);
  if (!pan_compressed) {
    position += sizeof(uint16_t);  // Source PAN ID
  }

  uint8_t extd_source[8] = {0};
  for (uint8_t idx = 0; idx < sizeof(extd_source); idx++) {
    extd_source[idx] = psdu[position + sizeof(extd_source) - 1 - idx];
  }
  position += sizeof(extd_source);

  snprintf(out->extd_source_str, sizeof(out->extd_source_str),
           ZB_ADDRESS_FORMAT, extd_source[0], extd_source[1], extd_source[2],
           extd_source[3], extd_source[4], extd_source[5], extd_source[6],
           extd_source[7]);

  if (out->frame_control == THREAD_FCF_MLE_PACKET) {
    if (frame_len < (position + 7)) {
      return false;
    }
    out->is_mle = true;
    strncpy(out->protocol_type, PROTOCOL_TYPE_MLE,
            sizeof(out->protocol_type) - 1);
    position += 3;  // Skip dispatch / headers
    out->udp_source_port =
        ((uint16_t) psdu[position] << 8) | psdu[position + 1];
    position += sizeof(uint16_t);
    out->udp_destination_port =
        ((uint16_t) psdu[position] << 8) | psdu[position + 1];
    position += sizeof(uint16_t);
  } else {
    out->is_mle = false;
    strncpy(out->protocol_type, PROTOCOL_TYPE_IEEE,
            sizeof(out->protocol_type) - 1);
  }

  return true;
}
