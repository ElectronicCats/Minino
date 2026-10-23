/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * VITA 49.2 Radio Transport packet implementations for ESP-SDR under ESP-IDF.
 */

#include "vrt_packet.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define NSEC_PER_SEC 1000000000ULL
#define PSEC_PER_SEC 1000000000000ULL

static inline uint32_t mskw(uint32_t val, uint32_t pos, uint32_t n) {
  uint32_t mask = (1U << n) - 1U;
  return (val & mask) << pos;
}

uint32_t vrt_pack_header(const vrt_header_t* hdr) {
  if (hdr == NULL) {
    return 0U;
  }
  uint32_t w = 0U;
  w |= mskw((uint32_t) hdr->packet_type, 28, 4);
  w |= mskw(hdr->has_class_id ? 1U : 0U, 27, 1);
  w |= mskw(hdr->has_trailer ? 1U : 0U, 26, 1);
  w |= mskw(hdr->nd0 ? 1U : 0U, 25, 1);
  w |= mskw((uint32_t) hdr->spectrum_or_time | (uint32_t) hdr->tsm, 24, 1);
  w |= mskw((uint32_t) hdr->tsi, 22, 2);
  w |= mskw((uint32_t) hdr->tsf, 20, 2);
  w |= mskw((uint32_t) (hdr->packet_count & 0x0fU), 16, 4);
  w |= (uint32_t) hdr->packet_size;
  return w;
}

bool vrt_unpack_header(uint32_t word, vrt_header_t* hdr) {
  if (hdr == NULL) {
    return false;
  }
  hdr->packet_type = (vrt_packet_type_t) ((word >> 28) & 0x0fU);
  hdr->has_class_id = (word & (1U << 27)) != 0U;
  hdr->has_trailer = (word & (1U << 26)) != 0U;
  hdr->nd0 = (word & (1U << 25)) != 0U;
  hdr->spectrum_or_time = (vrt_spectrum_or_time_t) ((word >> 24) & 0x01U);
  hdr->tsm = (vrt_tsm_t) ((word >> 24) & 0x01U);
  hdr->tsi = (vrt_tsi_t) ((word >> 22) & 0x03U);
  hdr->tsf = (vrt_tsf_t) ((word >> 20) & 0x03U);
  hdr->packet_count = (uint8_t) ((word >> 16) & 0x0fU);
  hdr->packet_size = (uint16_t) (word & 0xffffU);
  return true;
}

uint32_t vrt_pack_trailer(const vrt_trailer_t* tr) {
  if (tr == NULL) {
    return 0U;
  }
  uint32_t w = 0U;
  if (tr->has_calibrated_time) {
    w |= (1U << 31) | (tr->calibrated_time ? (1U << 19) : 0U);
  }
  if (tr->has_valid_data) {
    w |= (1U << 30) | (tr->valid_data ? (1U << 18) : 0U);
  }
  if (tr->has_reference_lock) {
    w |= (1U << 29) | (tr->reference_lock ? (1U << 17) : 0U);
  }
  if (tr->has_agc_or_mgc) {
    w |= (1U << 28) | (tr->agc_or_mgc ? (1U << 16) : 0U);
  }
  if (tr->has_detected_signal) {
    w |= (1U << 27) | (tr->detected_signal ? (1U << 15) : 0U);
  }
  if (tr->has_spectral_inversion) {
    w |= (1U << 26) | (tr->spectral_inversion ? (1U << 14) : 0U);
  }
  if (tr->has_over_range) {
    w |= (1U << 25) | (tr->over_range ? (1U << 13) : 0U);
  }
  if (tr->has_sample_loss) {
    w |= (1U << 24) | (tr->sample_loss ? (1U << 12) : 0U);
  }
  if (tr->has_context_packet_count) {
    w |= (1U << 7) | ((uint32_t) (tr->context_packet_count & 0x7fU));
  }
  return w;
}

bool vrt_unpack_trailer(uint32_t word, vrt_trailer_t* tr) {
  if (tr == NULL) {
    return false;
  }
  memset(tr, 0, sizeof(*tr));
  tr->has_calibrated_time = (word & (1U << 31)) != 0U;
  tr->calibrated_time = (word & (1U << 19)) != 0U;
  tr->has_valid_data = (word & (1U << 30)) != 0U;
  tr->valid_data = (word & (1U << 18)) != 0U;
  tr->has_reference_lock = (word & (1U << 29)) != 0U;
  tr->reference_lock = (word & (1U << 17)) != 0U;
  tr->has_agc_or_mgc = (word & (1U << 28)) != 0U;
  tr->agc_or_mgc = (word & (1U << 16)) != 0U;
  tr->has_detected_signal = (word & (1U << 27)) != 0U;
  tr->detected_signal = (word & (1U << 15)) != 0U;
  tr->has_spectral_inversion = (word & (1U << 26)) != 0U;
  tr->spectral_inversion = (word & (1U << 14)) != 0U;
  tr->has_over_range = (word & (1U << 25)) != 0U;
  tr->over_range = (word & (1U << 13)) != 0U;
  tr->has_sample_loss = (word & (1U << 24)) != 0U;
  tr->sample_loss = (word & (1U << 12)) != 0U;
  tr->has_context_packet_count = (word & (1U << 7)) != 0U;
  tr->context_packet_count = (uint8_t) (word & 0x7fU);
  return true;
}

void vrt_split_time(uint64_t ns,
                    uint32_t* integer_sec,
                    uint64_t* fractional_ps) {
  uint64_t sec = ns / NSEC_PER_SEC;
  uint64_t rem_ns = ns % NSEC_PER_SEC;
  if (integer_sec != NULL) {
    *integer_sec = (uint32_t) sec;
  }
  if (fractional_ps != NULL) {
    *fractional_ps = rem_ns * 1000ULL;
  }
}

void vrt_split_burst_time(uint64_t base_ns,
                          uint32_t sample_offset,
                          uint32_t rate_hz,
                          uint32_t* integer_sec,
                          uint64_t* fractional_ps) {
  uint64_t sec = base_ns / NSEC_PER_SEC;
  uint64_t rem_ns = base_ns % NSEC_PER_SEC;
  uint64_t ps = rem_ns * 1000ULL;

  if (rate_hz > 0U && sample_offset > 0U) {
    ps += (uint64_t) sample_offset * PSEC_PER_SEC / rate_hz;
  }
  if (ps >= PSEC_PER_SEC) {
    sec += ps / PSEC_PER_SEC;
    ps %= PSEC_PER_SEC;
  }
  if (integer_sec != NULL) {
    *integer_sec = (uint32_t) sec;
  }
  if (fractional_ps != NULL) {
    *fractional_ps = ps;
  }
}

size_t vrt_build_if_data_header(uint32_t* buf,
                                size_t max_words,
                                uint32_t stream_id,
                                uint8_t packet_count,
                                uint32_t integer_sec,
                                uint64_t fractional_ps,
                                size_t payload_words,
                                bool has_trailer) {
  if (buf == NULL || max_words < 5U) {
    return 0U;
  }
  uint16_t total_size =
      (uint16_t) (5U + payload_words + (has_trailer ? 1U : 0U));
  vrt_header_t hdr = {
      .packet_type = VRT_PT_IF_DATA_WITH_STREAM_ID,
      .has_class_id = false,
      .has_trailer = has_trailer,
      .nd0 = true, /* VITA 49.2 */
      .spectrum_or_time = VRT_SOT_TIME,
      .tsm = VRT_TSM_FINE,
      .tsi = VRT_TSI_OTHER,
      .tsf = VRT_TSF_REAL_TIME,
      .packet_count = (uint8_t) (packet_count & 0x0fU),
      .packet_size = total_size,
  };

  buf[0] = vrt_pack_header(&hdr);
  buf[1] = stream_id;
  buf[2] = integer_sec;
  buf[3] = (uint32_t) (fractional_ps >> 32U);
  buf[4] = (uint32_t) (fractional_ps & 0xffffffffULL);
  return 5U;
}

uint32_t vrt_build_if_data_trailer(bool valid_data,
                                   bool agc,
                                   bool clipped,
                                   bool sample_loss) {
  vrt_trailer_t tr = {
      .has_valid_data = true,
      .valid_data = valid_data,
      .has_agc_or_mgc = true,
      .agc_or_mgc = agc,
      .has_over_range = true,
      .over_range = clipped,
      .has_sample_loss = true,
      .sample_loss = sample_loss,
  };
  return vrt_pack_trailer(&tr);
}

void vrt_to_big_endian(uint32_t* words, size_t count) {
  if (words == NULL) {
    return;
  }
  for (size_t i = 0U; i < count; i++) {
    words[i] = __builtin_bswap32(words[i]);
  }
}

void vrt_from_big_endian(uint32_t* words, size_t count) {
  vrt_to_big_endian(words, count);
}

size_t vrt_build_data_packet(uint32_t* out_packet,
                             size_t max_words,
                             uint32_t stream_id,
                             uint8_t packet_count,
                             uint64_t timestamp_ns,
                             uint32_t sample_offset,
                             uint32_t rate_hz,
                             const struct esp_sdr_iq16* samples,
                             size_t num_samples,
                             unsigned int bits,
                             int32_t full_scale,
                             bool agc,
                             bool sample_loss,
                             bool network_byte_order) {
  if (out_packet == NULL || samples == NULL || num_samples == 0U ||
      bits == 0U || !iq_bits_valid(bits) || full_scale <= 0) {
    return 0U;
  }

  /* Check quantum alignment */
  size_t quantum = iq_sample_quantum(bits);
  if (quantum > 0U) {
    num_samples -= (num_samples % quantum);
  }
  if (num_samples == 0U) {
    return 0U;
  }

  /* Calculate sample clipping */
  bool clipped = false;
  for (size_t j = 0U; j < num_samples; j++) {
    int16_t si = samples[j].i;
    int16_t sq = samples[j].q;
    if (si >= full_scale - 1 || si <= -full_scale || sq >= full_scale - 1 ||
        sq <= -full_scale) {
      clipped = true;
      break;
    }
  }

  /* Reserve 5 header words at start, pack body afterwards */
  if (max_words < VRT_DATA_OVERHEAD_WORDS) {
    return 0U;
  }
  size_t payload_capacity = max_words - VRT_DATA_OVERHEAD_WORDS;
  uint32_t* body_ptr = &out_packet[5];
  size_t words_body = iq_pack(samples, num_samples, bits, full_scale, body_ptr);
  if (words_body == 0U || words_body > payload_capacity) {
    return 0U;
  }

  uint32_t sec = 0U;
  uint64_t ps = 0ULL;
  vrt_split_burst_time(timestamp_ns, sample_offset, rate_hz, &sec, &ps);

  vrt_build_if_data_header(out_packet, max_words, stream_id, packet_count, sec,
                           ps, words_body, true);

  /* Append trailer */
  out_packet[5U + words_body] =
      vrt_build_if_data_trailer(true, agc, clipped, sample_loss);

  size_t total_words = 5U + words_body + 1U;

  if (network_byte_order) {
    vrt_to_big_endian(out_packet, total_words);
  }
  return total_words;
}

static inline void write_double_words(double val, uint32_t* words) {
  union {
    double d;
    uint64_t u;
  } conv;
  conv.d = val;
  words[0] = (uint32_t) (conv.u >> 32U);
  words[1] = (uint32_t) (conv.u & 0xffffffffULL);
}

size_t vrt_build_context_packet(uint32_t* out_packet,
                                size_t max_words,
                                uint32_t stream_id,
                                uint8_t packet_count,
                                uint64_t timestamp_ns,
                                uint64_t freq_hz,
                                uint32_t rate_hz,
                                uint32_t bandwidth_hz,
                                unsigned int bits,
                                bool changed,
                                bool network_byte_order) {
  /*
   * Context Packet Layout:
   * Word 0: Header (1 word)
   * Word 1: Stream ID (1 word)
   * Word 2: Integer Seconds (1 word)
   * Word 3..4: Fractional Seconds (2 words)
   * Word 5: CIF0 (1 word)
   * Word 6: CIF1 (1 word)
   * Word 7..8: Bandwidth (2 words, 64-bit float)
   * Word 9..10: RF Reference Frequency (2 words, 64-bit float)
   * Word 11..12: Sample Rate (2 words, 64-bit float)
   * Word 13..14: Data Packet Payload Format (2 words)
   * Word 15: V49.2 Spec Compliance (1 word)
   * Total = 16 words.
   */
  if (out_packet == NULL || max_words < 16U || bits == 0U ||
      !iq_bits_valid(bits)) {
    return 0U;
  }

  uint16_t packet_size = 16U;
  vrt_header_t hdr = {
      .packet_type = VRT_PT_IF_CONTEXT,
      .has_class_id = false,
      .has_trailer = false,
      .nd0 = true, /* VITA 49.2 */
      .tsm = VRT_TSM_FINE,
      .tsi = VRT_TSI_OTHER,
      .tsf = VRT_TSF_REAL_TIME,
      .packet_count = (uint8_t) (packet_count & 0x0fU),
      .packet_size = packet_size,
  };

  uint32_t sec = 0U;
  uint64_t ps = 0ULL;
  vrt_split_time(timestamp_ns, &sec, &ps);

  out_packet[0] = vrt_pack_header(&hdr);
  out_packet[1] = stream_id;
  out_packet[2] = sec;
  out_packet[3] = (uint32_t) (ps >> 32U);
  out_packet[4] = (uint32_t) (ps & 0xffffffffULL);

  /* CIF0: Bit 31 (change), Bit 29 (BW), Bit 27 (RF freq), Bit 21 (Sample rate),
   * Bit 15 (payload fmt), Bit 0 (CIF1) */
  uint32_t cif0 = 0U;
  if (changed) {
    cif0 |= (1U << 31);
  }
  cif0 |= (1U << 29); /* Bandwidth */
  cif0 |= (1U << 27); /* RF Frequency */
  cif0 |= (1U << 21); /* Sample Rate */
  cif0 |= (1U << 15); /* Payload Format */
  cif0 |= (1U << 0);  /* CIF1 Enable */
  out_packet[5] = cif0;

  /* CIF1: Bit 3 (V49 Spec Compliance) */
  out_packet[6] = 0x00000008U;

  /* Bandwidth: 64-bit IEEE-754 double in Hz */
  write_double_words((double) bandwidth_hz, &out_packet[7]);

  /* RF Reference Frequency: 64-bit IEEE-754 double in Hz */
  write_double_words((double) freq_hz, &out_packet[9]);

  /* Sample Rate: 64-bit IEEE-754 double in Hz */
  write_double_words((double) rate_hz, &out_packet[11]);

  /* Data Packet Payload Format: Link Efficient (bit 31), Complex Cartesian
   * (bits 30..29 = 1), Signed Fixed Point (bits 28..24 = 0), Item Packing Field
   * Size (bits 11..6 = bits-1), Data Item Size (bits 5..0 = bits-1) */
  uint32_t fmt_w0 = (1U << 31) | (1U << 29) | (((bits - 1U) & 0x3fU) << 6) |
                    ((bits - 1U) & 0x3fU);
  out_packet[13] = fmt_w0;
  out_packet[14] = 0U; /* repeat_count = 0, vector_size = 0 */

  /* V49 Spec Compliance: VRT_V49_2 (2) */
  out_packet[15] = (uint32_t) VRT_V49_2;

  if (network_byte_order) {
    vrt_to_big_endian(out_packet, packet_size);
  }
  return packet_size;
}

size_t vrt_stream_packet_to_v49_data(const void* stream_pkt,
                                     uint32_t* vrt_buf,
                                     size_t max_words,
                                     uint32_t stream_id,
                                     uint8_t packet_count,
                                     int gain,
                                     bool sample_loss,
                                     bool network_byte_order) {
  if (stream_pkt == NULL || vrt_buf == NULL) {
    return 0U;
  }
  const esp_sdr_stream_pkt_t* sp = (const esp_sdr_stream_pkt_t*) stream_pkt;
  if (memcmp(sp->magic, "ESR2", 4) != 0 || sp->bits != 8U) {
    return 0U;
  }

  size_t num_samples = sizeof(sp->iq) / 2U; /* 672 samples */
  /* 672 samples at 8-bit link-efficient = 336 words */
  size_t payload_words = (num_samples * 16U) / 32U;
  size_t total_words = 5U + payload_words + 1U;
  if (max_words < total_words) {
    return 0U;
  }

  uint64_t time_ns = sp->time_us * 1000ULL;
  uint32_t sec = 0U;
  uint64_t ps = 0ULL;
  vrt_split_time(time_ns, &sec, &ps);

  vrt_build_if_data_header(vrt_buf, max_words, stream_id, packet_count, sec, ps,
                           payload_words, true);

  /* Pack 8-bit samples directly from stream packet into 32-bit words */
  uint32_t* body = &vrt_buf[5];
  const uint8_t* raw_iq = (const uint8_t*) sp->iq;
  bool clipped = false;

  for (size_t i = 0U, w = 0U; i < num_samples; i += 2U, w++) {
    int8_t i0 = (int8_t) raw_iq[2U * i];
    int8_t q0 = (int8_t) raw_iq[2U * i + 1U];
    int8_t i1 = (int8_t) raw_iq[2U * (i + 1U)];
    int8_t q1 = (int8_t) raw_iq[2U * (i + 1U) + 1U];

    if (i0 == 127 || i0 == -128 || q0 == 127 || q0 == -128 || i1 == 127 ||
        i1 == -128 || q1 == 127 || q1 == -128) {
      clipped = true;
    }

    body[w] = ((uint32_t) (uint8_t) i0 << 24) |
              ((uint32_t) (uint8_t) q0 << 16) | ((uint32_t) (uint8_t) i1 << 8) |
              ((uint32_t) (uint8_t) q1);
  }

  bool agc = (gain < 0);
  vrt_buf[5U + payload_words] =
      vrt_build_if_data_trailer(true, agc, clipped, sample_loss);

  if (network_byte_order) {
    vrt_to_big_endian(vrt_buf, total_words);
  }
  return total_words;
}
